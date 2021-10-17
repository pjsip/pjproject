/* $Id$ */
/*
 * Copyright (C) 2015-2016 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2012-2015 Zaark Technology AB
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
/* This file is the implementation of Opus codec wrapper and was contributed by
 * Zaark Technology AB
 */

#include <pjmedia-codec/opus.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pj/log.h>
#include <pj/math.h>

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC!=0)

#include <opus/opus.h>

#define THIS_FILE "opus.c"

/* Default packet loss concealment setting. */
#define OPUS_DEFAULT_PLC	1
/* Default Voice Activity Detector setting. */
#define OPUS_DEFAULT_VAD	0

/* Maximum size of an encoded packet. 
 * If the the actual size is bigger, the encode/parse will fail.
 */
#define MAX_ENCODED_PACKET_SIZE 	1280

/* Default frame time (msec) */
#define PTIME			20

/* Tracing */
#if 0
#   define TRACE_(expr)	PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif


/* Prototypes for Opus factory */
static pj_status_t factory_test_alloc( pjmedia_codec_factory *factory, 
				       const pjmedia_codec_info *ci );
static pj_status_t factory_default_attr( pjmedia_codec_factory *factory, 
					 const pjmedia_codec_info *ci, 
					 pjmedia_codec_param *attr );
static pj_status_t factory_enum_codecs( pjmedia_codec_factory *factory, 
					unsigned *count, 
					pjmedia_codec_info codecs[]);
static pj_status_t factory_alloc_codec( pjmedia_codec_factory *factory, 
					const pjmedia_codec_info *ci, 
					pjmedia_codec **p_codec);
static pj_status_t factory_dealloc_codec( pjmedia_codec_factory *factory, 
					  pjmedia_codec *codec );


/* Prototypes for Opus implementation. */
static pj_status_t codec_init( pjmedia_codec *codec, 
			       pj_pool_t *pool );
static pj_status_t codec_open( pjmedia_codec *codec, 
			       pjmedia_codec_param *attr );
static pj_status_t codec_close( pjmedia_codec *codec );
static pj_status_t codec_modify( pjmedia_codec *codec, 
				 const pjmedia_codec_param *attr );
static pj_status_t codec_parse( pjmedia_codec *codec,
				void *pkt,
				pj_size_t pkt_size,
				const pj_timestamp *ts,
				unsigned *frame_cnt,
				pjmedia_frame frames[]);
static pj_status_t codec_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
static pj_status_t codec_decode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output);
static pj_status_t codec_recover( pjmedia_codec *codec,
				  unsigned output_buf_len,
				  struct pjmedia_frame *output);

/* Definition for Opus operations. */
static pjmedia_codec_op opus_op = 
{
    &codec_init,
    &codec_open,
    &codec_close,
    &codec_modify,
    &codec_parse,
    &codec_encode,
    &codec_decode,
    &codec_recover
};

/* Definition for Opus factory operations. */
static pjmedia_codec_factory_op opus_factory_op =
{
    &factory_test_alloc,
    &factory_default_attr,
    &factory_enum_codecs,
    &factory_alloc_codec,
    &factory_dealloc_codec,
    &pjmedia_codec_opus_deinit
};


/* Opus factory */
struct opus_codec_factory
{
    pjmedia_codec_factory  base;
    pjmedia_endpt	  *endpt;
    pj_pool_t		  *pool;
};

/* Opus codec private data. */
struct opus_data
{
    pj_pool_t         		*pool;
    pj_mutex_t        		*mutex;
    OpusEncoder 		*enc;
    OpusDecoder       		*dec;
    OpusRepacketizer  		*enc_packer;
    OpusRepacketizer  		*dec_packer;
    pjmedia_codec_opus_config 	 cfg;
    unsigned   			 enc_ptime;
    unsigned			 dec_ptime;
    pjmedia_frame      		 dec_frame[2];
    int                		 dec_frame_index;
};

/* Codec factory instance */
static struct opus_codec_factory opus_codec_factory;

/* Opus default configuration */
static pjmedia_codec_opus_config opus_cfg =
{
    PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE,     /* Sample rate		*/
    1,						/* Channel count	*/
    PTIME,					/* Frame time 		*/			
    PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE,	/* Bit rate             */
    5,						/* Expected packet loss */
    PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY,	/* Complexity           */
    PJMEDIA_CODEC_OPUS_DEFAULT_CBR,		/* Constant bit rate    */
};


static int get_opus_bw_constant (unsigned sample_rate)
{
    if (sample_rate <= 8000)
	return OPUS_BANDWIDTH_NARROWBAND;
    else if (sample_rate <= 12000)
	return OPUS_BANDWIDTH_MEDIUMBAND;
    else if (sample_rate <= 16000)
	return OPUS_BANDWIDTH_WIDEBAND;
    else if (sample_rate <= 24000)
	return OPUS_BANDWIDTH_SUPERWIDEBAND;
    else
	return OPUS_BANDWIDTH_FULLBAND;
}


/*
 * Initialize and register Opus codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_opus_init( pjmedia_endpt *endpt )
{
    pj_status_t status;
    pjmedia_codec_mgr *codec_mgr;

    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    if (opus_codec_factory.pool != NULL)
	return PJ_SUCCESS;

    /* Create the Opus codec factory */
    opus_codec_factory.base.op           = &opus_factory_op;
    opus_codec_factory.base.factory_data = &opus_codec_factory;
    opus_codec_factory.endpt             = endpt;

    opus_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "opus-factory",
    							1024, 1024);
    if (!opus_codec_factory.pool) {
	PJ_LOG(2, (THIS_FILE, "Unable to create memory pool for Opus codec"));
	return PJ_ENOMEM;
    }

    /* Get the codec manager */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	PJ_LOG(2, (THIS_FILE, "Unable to get the codec manager"));
	status = PJ_EINVALIDOP;
	goto on_codec_factory_error;
    }

    /* Register the codec factory */
    status = pjmedia_codec_mgr_register_factory (codec_mgr,
						 &opus_codec_factory.base);
    if (status != PJ_SUCCESS) {
	PJ_LOG(2, (THIS_FILE, "Unable to register the codec factory"));
	goto on_codec_factory_error;
    }

    return PJ_SUCCESS;

on_codec_factory_error:
    pj_pool_release(opus_codec_factory.pool);
    opus_codec_factory.pool = NULL;
    return status;
}


/*
 * Unregister Opus codec factory from pjmedia endpoint and
 * deinitialize the codec.
 */
PJ_DEF(pj_status_t) pjmedia_codec_opus_deinit( void )
{
    pj_status_t status;
    pjmedia_codec_mgr *codec_mgr;

    if (opus_codec_factory.pool == NULL)
	return PJ_SUCCESS;

    /* Get the codec manager */
    codec_mgr = pjmedia_endpt_get_codec_mgr(opus_codec_factory.endpt);
    if (!codec_mgr) {
	PJ_LOG(2, (THIS_FILE, "Unable to get the codec manager"));
	pj_pool_release(opus_codec_factory.pool);
	opus_codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister the codec factory */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &opus_codec_factory.base);
    if (status != PJ_SUCCESS)
	PJ_LOG(2, (THIS_FILE, "Unable to unregister the codec factory"));

    /* Release the memory pool */
    pj_pool_release(opus_codec_factory.pool);
    opus_codec_factory.pool = NULL;

    return status;
}


/**
 * Get the opus configuration for a specific sample rate.
 */
PJ_DEF(pj_status_t)
pjmedia_codec_opus_get_config( pjmedia_codec_opus_config *cfg )
{
    PJ_ASSERT_RETURN(cfg, PJ_EINVAL);

    pj_memcpy(cfg, &opus_cfg, sizeof(pjmedia_codec_opus_config));
    return PJ_SUCCESS;
}


static pj_str_t STR_MAX_PLAYBACK = {"maxplaybackrate", 15};
static pj_str_t STR_MAX_CAPTURE  = {"sprop-maxcapturerate", 20};
static pj_str_t STR_STEREO  	 = {"stereo", 6};
static pj_str_t STR_SPROP_STEREO = {"sprop-stereo", 12};
static pj_str_t STR_MAX_BIT_RATE = {"maxaveragebitrate", 17};
static pj_str_t STR_INBAND_FEC   = {"useinbandfec", 12};
static pj_str_t STR_DTX          = {"usedtx", 6};
static pj_str_t STR_CBR          = {"cbr", 3};

static int find_fmtp(pjmedia_codec_fmtp *fmtp, pj_str_t *name, pj_bool_t add)
{
    int i;
    for (i = 0; i < fmtp->cnt; i++) {
    	if (pj_stricmp(&fmtp->param[i].name, name) == 0)
	    return i;
    }
    
    if (add && (i < PJMEDIA_CODEC_MAX_FMTP_CNT)) {
        fmtp->param[i].name = *name;
        fmtp->cnt++;
        return i;
    } else
        return -1;
}

static void remove_fmtp(pjmedia_codec_fmtp *fmtp, pj_str_t *name)
{
    int i, j;
    for (i = 0; i < fmtp->cnt; i++) {
    	if (pj_stricmp(&fmtp->param[i].name, name) == 0) {
    	    fmtp->cnt--;
    	    for (j = i; j < fmtp->cnt; j++) {
    	    	fmtp->param[i].name = fmtp->param[i+1].name;
    	    	fmtp->param[i].val = fmtp->param[i+1].val;
    	    }
    	}
    }
}

static pj_status_t generate_fmtp(pjmedia_codec_param *attr)
{
    int idx;
    static char bitrate_str[12];
    static char clockrate_str[12];
    
    if (attr->info.clock_rate != 48000) {
	pj_ansi_snprintf(clockrate_str, sizeof(clockrate_str), "%u",
			 attr->info.clock_rate);

        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_MAX_PLAYBACK, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str(clockrate_str);

	idx = find_fmtp(&attr->setting.dec_fmtp, &STR_MAX_CAPTURE, PJ_TRUE);
	if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str(clockrate_str);
    } else {
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_MAX_PLAYBACK);
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_MAX_CAPTURE);
    }

    /* Check if we need to set parameter 'maxaveragebitrate' */
    if (opus_cfg.bit_rate > 0) {
        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_MAX_BIT_RATE, PJ_TRUE);
        if (idx >= 0) {
	    pj_ansi_snprintf(bitrate_str, sizeof(bitrate_str), "%u",
			     attr->info.avg_bps);
	    attr->setting.dec_fmtp.param[idx].val = pj_str(bitrate_str);
	}
    } else {
        remove_fmtp(&attr->setting.dec_fmtp, &STR_MAX_BIT_RATE);
    }

    if (attr->info.channel_cnt > 1) {
        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_STEREO, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str("1");

        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_SPROP_STEREO, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str("1");
    } else {
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_STEREO);
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_SPROP_STEREO);
    }

    if (opus_cfg.cbr) {
        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_CBR, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str("1");
    } else {
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_CBR);
    }

    if (attr->setting.plc) {
        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_INBAND_FEC, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str("1");
    } else {
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_INBAND_FEC);
    }

    if (attr->setting.vad) {
        idx = find_fmtp(&attr->setting.dec_fmtp, &STR_DTX, PJ_TRUE);
        if (idx >= 0)
	    attr->setting.dec_fmtp.param[idx].val = pj_str("1");
    } else {
    	remove_fmtp(&attr->setting.dec_fmtp, &STR_DTX);
    }
    
    return PJ_SUCCESS;
}

/**
 * Set the opus configuration and default param.
 */
PJ_DEF(pj_status_t)
pjmedia_codec_opus_set_default_param(const pjmedia_codec_opus_config *cfg,
				     pjmedia_codec_param *param )
{
    const pj_str_t opus_str = {"opus", 4};
    const pjmedia_codec_info *info[1];
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 1;
    pj_status_t status;

    TRACE_((THIS_FILE, "%s:%d: - TRACE", __FUNCTION__, __LINE__));
    PJ_ASSERT_RETURN(cfg && param, PJ_EINVAL);

    codec_mgr = pjmedia_endpt_get_codec_mgr(opus_codec_factory.endpt);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, &opus_str,
						 &count, info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Set sample rate */
    if (cfg->sample_rate != 8000 && cfg->sample_rate != 12000 &&
	cfg->sample_rate != 16000 && cfg->sample_rate != 24000 &&
	cfg->sample_rate != 48000)
    {
	return PJ_EINVAL;
    }

    param->info.clock_rate = opus_cfg.sample_rate = cfg->sample_rate;
    param->info.max_bps = opus_cfg.sample_rate * 2;
    opus_cfg.frm_ptime = cfg->frm_ptime;
    param->info.frm_ptime = (pj_uint16_t)cfg->frm_ptime;

    /* Set channel count */
    if (cfg->channel_cnt != 1 && cfg->channel_cnt != 2)
        return PJ_EINVAL;
    param->info.channel_cnt = opus_cfg.channel_cnt = cfg->channel_cnt;

    /* Set bit_rate */
    if ((cfg->bit_rate != PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE) && 
       (cfg->bit_rate < 6000 || cfg->bit_rate > 510000)) 
    {
	return PJ_EINVAL;
    }
    opus_cfg.bit_rate = cfg->bit_rate;
    param->info.avg_bps = opus_cfg.bit_rate;

    /* Set expected packet loss */
    if (cfg->packet_loss >= 100)
	return PJ_EINVAL;
    opus_cfg.packet_loss = cfg->packet_loss;

    /* Set complexity */
    if (cfg->complexity > 10)
	return PJ_EINVAL;
    opus_cfg.complexity = cfg->complexity;

    opus_cfg.cbr = cfg->cbr;
    
    generate_fmtp(param);

    status = pjmedia_codec_mgr_set_default_param(codec_mgr, info[0], param);
    return status;
}


/*
 * Check if factory can allocate the specified codec.
 */
static pj_status_t factory_test_alloc( pjmedia_codec_factory *factory, 
				       const pjmedia_codec_info *ci )
{
    const pj_str_t opus_tag = {"OPUS", 4};

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(factory==&opus_codec_factory.base, PJ_EINVAL);

    /* Type MUST be audio. */
    if (ci->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp(&ci->encoding_name, &opus_tag) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock rate */
    if (ci->clock_rate != 8000 && ci->clock_rate != 12000 &&
	ci->clock_rate != 16000 && ci->clock_rate != 24000 &&
	ci->clock_rate != 48000)
    {
	return PJMEDIA_CODEC_EUNSUP;
    }

    return PJ_SUCCESS;
}


/*
 * Generate default attribute.
 */
static pj_status_t factory_default_attr( pjmedia_codec_factory *factory, 
					 const pjmedia_codec_info *ci, 
					 pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);
    TRACE_((THIS_FILE, "%s:%d: - TRACE", __FUNCTION__, __LINE__));

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.pt          	   = (pj_uint8_t)ci->pt;
    attr->info.clock_rate  	   = opus_cfg.sample_rate;
    attr->info.channel_cnt 	   = opus_cfg.channel_cnt;
    attr->info.avg_bps     	   = opus_cfg.bit_rate;
    attr->info.max_bps     	   = opus_cfg.sample_rate * 2;
    attr->info.frm_ptime   	   = (pj_uint16_t)opus_cfg.frm_ptime;
    attr->setting.frm_per_pkt 	   = 1;
    attr->info.pcm_bits_per_sample = 16;
    attr->setting.vad      	   = OPUS_DEFAULT_VAD;
    attr->setting.plc      	   = OPUS_DEFAULT_PLC;

    /* Set max RX frame size to 1275 (max Opus frame size) to anticipate
     * possible ptime change on the fly.
     */
    attr->info.max_rx_frame_size   = 1275;

    generate_fmtp(attr);

    return PJ_SUCCESS;
}


/*
 * Enum codecs supported by this factory.
 */
static pj_status_t factory_enum_codecs( pjmedia_codec_factory *factory, 
					unsigned *count, 
					pjmedia_codec_info codecs[] )
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs, PJ_EINVAL);

    if (*count > 0) {
	pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
	codecs[0].type          = PJMEDIA_TYPE_AUDIO;
	codecs[0].pt            = PJMEDIA_RTP_PT_OPUS;
        /*
         * RFC 7587, Section 7:
         * The media subtype ("opus") goes in SDP "a=rtpmap" as the encoding
         * name. The RTP clock rate in "a=rtpmap" MUST be 48000 and the
         * number of channels MUST be 2.
         */	
	codecs[0].encoding_name = pj_str("opus");
	codecs[0].clock_rate    = 48000;
	codecs[0].channel_cnt   = 2;
	*count = 1;
    }

    return PJ_SUCCESS;
}


/*
 * Allocate a new Opus codec instance.
 */
static pj_status_t factory_alloc_codec( pjmedia_codec_factory *factory, 
					const pjmedia_codec_info *ci, 
					pjmedia_codec **p_codec )
{
    pjmedia_codec *codec;
    pj_pool_t *pool;
    pj_status_t status;
    struct opus_data *opus_data;
    struct opus_codec_factory *f = (struct opus_codec_factory*) factory;

    PJ_UNUSED_ARG(ci);
    TRACE_((THIS_FILE, "%s:%d: - TRACE", __FUNCTION__, __LINE__));

    pool = pjmedia_endpt_create_pool(f->endpt, "opus", 512, 512);
    if (!pool) return PJ_ENOMEM;
    
    opus_data = PJ_POOL_ZALLOC_T(pool, struct opus_data);
    codec     = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);

    status = pj_mutex_create_simple (pool, "opus_mutex", &opus_data->mutex);
    if (status != PJ_SUCCESS) {
    	pj_pool_release(pool);
    	return status;
    }

    pj_memcpy(&opus_data->cfg, &opus_cfg, sizeof(pjmedia_codec_opus_config));
    opus_data->pool      = pool;
    codec->op            = &opus_op;
    codec->factory       = factory;
    codec->codec_data    = opus_data;

    *p_codec = codec;
    return PJ_SUCCESS;
}


/*
 * Free codec.
 */
static pj_status_t factory_dealloc_codec( pjmedia_codec_factory *factory, 
					  pjmedia_codec *codec )
{
    struct opus_data *opus_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &opus_codec_factory.base, PJ_EINVAL);

    opus_data = (struct opus_data *)codec->codec_data;
    if (opus_data) {
        pj_mutex_destroy(opus_data->mutex);
        opus_data->mutex = NULL;
	pj_pool_release(opus_data->pool);
    }

    return PJ_SUCCESS;
}


/*
 * Init codec.
 */
static pj_status_t codec_init( pjmedia_codec *codec, 
			       pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}


/*
 * Open codec.
 */
static pj_status_t  codec_open( pjmedia_codec *codec,
				pjmedia_codec_param *attr )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;
    int idx, err;
    pj_bool_t auto_bit_rate = PJ_TRUE;

    PJ_ASSERT_RETURN(codec && attr && opus_data, PJ_EINVAL);

    pj_mutex_lock (opus_data->mutex);

    TRACE_((THIS_FILE, "%s:%d: - TRACE", __FUNCTION__, __LINE__));

    opus_data->cfg.sample_rate = attr->info.clock_rate;
    opus_data->cfg.channel_cnt = attr->info.channel_cnt;
    opus_data->enc_ptime = opus_data->dec_ptime = attr->info.frm_ptime;

    /* Allocate memory used by the codec */
    if (!opus_data->enc) {
	/* Allocate memory for max 2 channels */
	opus_data->enc = pj_pool_zalloc(opus_data->pool,
					opus_encoder_get_size(2));
    }
    if (!opus_data->dec) {
	/* Allocate memory for max 2 channels */
	opus_data->dec = pj_pool_zalloc(opus_data->pool,
					opus_decoder_get_size(2));
    }
    if (!opus_data->enc_packer) {
	opus_data->enc_packer = pj_pool_zalloc(opus_data->pool,
					       opus_repacketizer_get_size());
    }
    if (!opus_data->dec_packer) {
	opus_data->dec_packer = pj_pool_zalloc(opus_data->pool,
					       opus_repacketizer_get_size());
    }
    if (!opus_data->enc || !opus_data->dec ||
	!opus_data->enc_packer || !opus_data->dec_packer)
    {
	PJ_LOG(2, (THIS_FILE, "Unable to allocate memory for the codec"));
        pj_mutex_unlock (opus_data->mutex);
	return PJ_ENOMEM;
    }

    /* Check max average bit rate */
    idx = find_fmtp(&attr->setting.enc_fmtp, &STR_MAX_BIT_RATE, PJ_FALSE);
    if (idx >= 0) {
	unsigned rate;
	auto_bit_rate = PJ_FALSE;
	rate = (unsigned)pj_strtoul(&attr->setting.enc_fmtp.param[idx].val);
	if (rate < attr->info.avg_bps)
	    attr->info.avg_bps = rate;
    }

    /* Check plc */
    idx = find_fmtp(&attr->setting.enc_fmtp, &STR_INBAND_FEC, PJ_FALSE);
    if (idx >= 0) {
	unsigned plc;
	plc = (unsigned) pj_strtoul(&attr->setting.enc_fmtp.param[idx].val);
	attr->setting.plc = plc > 0? PJ_TRUE: PJ_FALSE;
    }

    /* Check vad */
    idx = find_fmtp(&attr->setting.enc_fmtp, &STR_DTX, PJ_FALSE);
    if (idx >= 0) {
	unsigned vad;
	vad = (unsigned) pj_strtoul(&attr->setting.enc_fmtp.param[idx].val);
	attr->setting.vad = vad > 0? PJ_TRUE: PJ_FALSE;
    }

    /* Check cbr */
    idx = find_fmtp(&attr->setting.enc_fmtp, &STR_CBR, PJ_FALSE);
    if (idx >= 0) {
	unsigned cbr;
	cbr = (unsigned) pj_strtoul(&attr->setting.enc_fmtp.param[idx].val);
	opus_data->cfg.cbr = cbr > 0? PJ_TRUE: PJ_FALSE;
    }
    
    /* Check max average bit rate */
    idx = find_fmtp(&attr->setting.dec_fmtp, &STR_MAX_BIT_RATE, PJ_FALSE);
    if (idx >= 0) {
	unsigned rate;
	rate = (unsigned) pj_strtoul(&attr->setting.dec_fmtp.param[idx].val);
	if (rate < attr->info.avg_bps)
	    attr->info.avg_bps = rate;
    }

    TRACE_((THIS_FILE, "%s:%d: sample_rate: %u",
	    __FUNCTION__, __LINE__, opus_data->cfg.sample_rate));

    /* Initialize encoder */
    err = opus_encoder_init(opus_data->enc,
			    opus_data->cfg.sample_rate,
			    attr->info.channel_cnt,
			    OPUS_APPLICATION_VOIP);
    if (err != OPUS_OK) {
	PJ_LOG(2, (THIS_FILE, "Unable to create encoder"));
	pj_mutex_unlock (opus_data->mutex);
	return PJMEDIA_CODEC_EFAILED;
    }
    
    /* Set signal type */
    opus_encoder_ctl(opus_data->enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    /* Set bitrate */
    opus_encoder_ctl(opus_data->enc, OPUS_SET_BITRATE(auto_bit_rate?
    						      OPUS_AUTO:
    						      attr->info.avg_bps));
    /* Set VAD */
    opus_encoder_ctl(opus_data->enc, OPUS_SET_DTX(attr->setting.vad ? 1 : 0));
    /* Set PLC */
    opus_encoder_ctl(opus_data->enc,
    		     OPUS_SET_INBAND_FEC(attr->setting.plc ? 1 : 0));
    /* Set bandwidth */
    opus_encoder_ctl(opus_data->enc,
    		     OPUS_SET_MAX_BANDWIDTH(get_opus_bw_constant(
    					    opus_data->cfg.sample_rate)));
    /* Set expected packet loss */
    opus_encoder_ctl(opus_data->enc,
    		OPUS_SET_PACKET_LOSS_PERC(opus_data->cfg.packet_loss));
    /* Set complexity */
    opus_encoder_ctl(opus_data->enc,
		     OPUS_SET_COMPLEXITY(opus_data->cfg.complexity));
    /* Set constant bit rate */
    opus_encoder_ctl(opus_data->enc,
    		     OPUS_SET_VBR(opus_data->cfg.cbr ? 0 : 1));

    PJ_LOG(5, (THIS_FILE, "Initialize Opus encoder, sample rate: %d, "
    			  "avg bitrate: %d, vad: %d, plc: %d, pkt loss: %d, "
    			  "complexity: %d, constant bit rate: %d",
               		  opus_data->cfg.sample_rate,
               		  attr->info.avg_bps, attr->setting.vad?1:0,
               		  attr->setting.plc?1:0,
               		  opus_data->cfg.packet_loss,
               		  opus_data->cfg.complexity,
               		  opus_data->cfg.cbr?1:0));

    /* Initialize decoder */
    err = opus_decoder_init (opus_data->dec,
			     opus_data->cfg.sample_rate,
			     attr->info.channel_cnt);
    if (err != OPUS_OK) {
	PJ_LOG(2, (THIS_FILE, "Unable to initialize decoder"));
	pj_mutex_unlock (opus_data->mutex);
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Initialize temporary decode frames used for FEC */
    opus_data->dec_frame[0].type = PJMEDIA_FRAME_TYPE_NONE;
    opus_data->dec_frame[0].buf  = pj_pool_zalloc(opus_data->pool,                                   
        	(opus_data->cfg.sample_rate / 1000)
                * 60 * attr->info.channel_cnt * 2 /* bytes per sample */);
    opus_data->dec_frame[1].type = PJMEDIA_FRAME_TYPE_NONE;
    opus_data->dec_frame[1].buf  = pj_pool_zalloc(opus_data->pool,
		(opus_data->cfg.sample_rate / 1000)
                * 60 * attr->info.channel_cnt * 2 /* bytes per sample */);
    opus_data->dec_frame_index = -1;

    /* Initialize the repacketizers */
    opus_repacketizer_init(opus_data->enc_packer);
    opus_repacketizer_init(opus_data->dec_packer);

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}


/*
 * Close codec.
 */
static pj_status_t  codec_close( pjmedia_codec *codec )
{
    PJ_UNUSED_ARG(codec);
    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  codec_modify( pjmedia_codec *codec, 
				  const pjmedia_codec_param *attr )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;

    pj_mutex_lock (opus_data->mutex);

    TRACE_((THIS_FILE, "%s:%d: - TRACE", __FUNCTION__, __LINE__));

    /* Set bitrate */
    opus_data->cfg.bit_rate = attr->info.avg_bps;
    opus_encoder_ctl(opus_data->enc, OPUS_SET_BITRATE(attr->info.avg_bps?
    						      attr->info.avg_bps:
    						      OPUS_AUTO));
    /* Set VAD */
    opus_encoder_ctl(opus_data->enc, OPUS_SET_DTX(attr->setting.vad ? 1 : 0));
    /* Set PLC */
    opus_encoder_ctl(opus_data->enc,
    		     OPUS_SET_INBAND_FEC(attr->setting.plc ? 1 : 0));

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  codec_parse( pjmedia_codec *codec,
				 void *pkt,
				 pj_size_t pkt_size,
				 const pj_timestamp *ts,
				 unsigned *frame_cnt,
				 pjmedia_frame frames[] )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;
    unsigned char tmp_buf[MAX_ENCODED_PACKET_SIZE];
    int i, num_frames;
    int size, out_pos;
    unsigned samples_per_frame = 0;
#if (USE_INCOMING_WORSE_SETTINGS)
    int bw;
#endif

    pj_mutex_lock (opus_data->mutex);

    if (pkt_size > sizeof(tmp_buf)) {
	PJ_LOG(5, (THIS_FILE, "Encoded size bigger than buffer"));
        pj_mutex_unlock (opus_data->mutex);
	return PJMEDIA_CODEC_EFRMTOOSHORT;
    }

    pj_memcpy(tmp_buf, pkt, pkt_size);

    opus_repacketizer_init(opus_data->dec_packer);
    opus_repacketizer_cat(opus_data->dec_packer, tmp_buf, pkt_size);

    num_frames = opus_repacketizer_get_nb_frames(opus_data->dec_packer);
    out_pos = 0;
    for (i = 0; i < num_frames; ++i) {
	size = opus_repacketizer_out_range(opus_data->dec_packer, i, i+1,
					   ((unsigned char*)pkt) + out_pos,
					   sizeof(tmp_buf));
	if (size < 0) {
	    PJ_LOG(5, (THIS_FILE, "Parse failed! (%d)", pkt_size));
            pj_mutex_unlock (opus_data->mutex);
	    return PJMEDIA_CODEC_EFAILED;
	}
	frames[i].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[i].buf = ((char*)pkt) + out_pos;
	frames[i].size = size;
	frames[i].bit_info = opus_packet_get_nb_samples(frames[i].buf,
			     frames[i].size, opus_data->cfg.sample_rate);

	if (i == 0) {
    	    unsigned ptime = frames[i].bit_info * 1000 /
    	    		     opus_data->cfg.sample_rate;
    	    if (ptime != opus_data->dec_ptime) {
             	PJ_LOG(4, (THIS_FILE, "Opus ptime change detected: %d ms "
             			      "--> %d ms",
             			      opus_data->dec_ptime, ptime));
        	opus_data->dec_ptime = ptime;
        	opus_data->dec_frame_index = -1;

        	/* Signal to the stream about ptime change. */
     	    	frames[i].bit_info |= 0x10000;
    	    }
     	    samples_per_frame = frames[i].bit_info;
   	}

	frames[i].timestamp.u64 = ts->u64 + i * samples_per_frame;
	out_pos += size;
    }
    *frame_cnt = num_frames;

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}


/*
 * Encode frame.
 */
static pj_status_t codec_encode( pjmedia_codec *codec, 
				 const struct pjmedia_frame *input,
				 unsigned output_buf_len, 
				 struct pjmedia_frame *output )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;
    opus_int32 size  = 0;
    unsigned in_pos  = 0;
    unsigned out_pos = 0;
    unsigned frame_size;
    unsigned samples_per_frame;
    unsigned char tmp_buf[MAX_ENCODED_PACKET_SIZE];
    unsigned tmp_bytes_left = sizeof(tmp_buf);

    pj_mutex_lock (opus_data->mutex);

    samples_per_frame = (opus_data->cfg.sample_rate *
			 opus_data->enc_ptime) / 1000;
    frame_size = samples_per_frame * opus_data->cfg.channel_cnt *
    		 sizeof(opus_int16);

    opus_repacketizer_init(opus_data->enc_packer);
    while (input->size - in_pos >= frame_size) {
	size = opus_encode(opus_data->enc,
			   (const opus_int16*)(((char*)input->buf) + in_pos),
			   samples_per_frame,
			   tmp_buf + out_pos,
			   (tmp_bytes_left < frame_size ?
			    tmp_bytes_left : frame_size));
	if (size < 0) {
	    PJ_LOG(4, (THIS_FILE, "Encode failed! (%d)", size));
            pj_mutex_unlock (opus_data->mutex);
	    return PJMEDIA_CODEC_EFAILED;
	} else if (size > 0) {
	    /* Only add packets containing more than the TOC */
	    opus_repacketizer_cat(opus_data->enc_packer,
				  tmp_buf + out_pos,
				  size);
	    out_pos += size;
	    tmp_bytes_left -= size;
	}
	in_pos += frame_size;
    }

    if (!opus_repacketizer_get_nb_frames(opus_data->enc_packer)) {
	/* Empty packet */
	output->size      = 0;
	output->type      = PJMEDIA_FRAME_TYPE_NONE;
	output->timestamp = input->timestamp;
    }

    if (size) {
	size = opus_repacketizer_out(opus_data->enc_packer,
				     output->buf,
				     output_buf_len);
	if (size < 0) {
	    PJ_LOG(4, (THIS_FILE, "Encode failed! (%d), out_size: %u",
	    			  size, output_buf_len));
	    pj_mutex_unlock (opus_data->mutex);
	    return PJMEDIA_CODEC_EFAILED;
	}
    }

    output->size      = (unsigned)size;
    output->type      = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}


/*
 * Decode frame.
 */
static pj_status_t  codec_decode( pjmedia_codec *codec, 
				  const struct pjmedia_frame *input,
				  unsigned output_buf_len, 
				  struct pjmedia_frame *output )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;
    int decoded_samples;
    pjmedia_frame *inframe;
    int fec = 0;
    int frm_size;

    PJ_UNUSED_ARG(output_buf_len);

    pj_mutex_lock (opus_data->mutex);

    if (opus_data->dec_frame_index == -1) {
        /* First packet, buffer it. */
        opus_data->dec_frame[0].type = input->type;
        opus_data->dec_frame[0].size = input->size;
        opus_data->dec_frame[0].timestamp = input->timestamp;
        pj_memcpy(opus_data->dec_frame[0].buf, input->buf, input->size);
        opus_data->dec_frame_index = 0;
        pj_mutex_unlock (opus_data->mutex);

        /* Return zero decoded bytes */
        output->size = 0;
        output->type = PJMEDIA_FRAME_TYPE_NONE;
        output->timestamp = input->timestamp;

        return PJ_SUCCESS;
    }

    inframe = &opus_data->dec_frame[opus_data->dec_frame_index];

    if (inframe->type != PJMEDIA_FRAME_TYPE_AUDIO) {
        /* Update current frame index */
        opus_data->dec_frame_index++;
        if (opus_data->dec_frame_index > 1)
            opus_data->dec_frame_index = 0;
        /* Copy original input buffer to current indexed frame */
        inframe = &opus_data->dec_frame[opus_data->dec_frame_index];
        inframe->type = input->type;
        inframe->size = input->size;
        inframe->timestamp = input->timestamp;
        pj_memcpy(inframe->buf, input->buf, input->size);
        fec = 1;
    }

    /* From Opus doc: In the case of PLC (data==NULL) or FEC(decode_fec=1),
     * then frame_size needs to be exactly the duration of audio that
     * is missing.
     */
    frm_size = output->size / (sizeof(opus_int16) *
               opus_data->cfg.channel_cnt);
    if (inframe->type != PJMEDIA_FRAME_TYPE_AUDIO || fec) {
	frm_size = PJ_MIN((unsigned)frm_size,
			  opus_data->cfg.sample_rate *
			  opus_data->dec_ptime / 1000);
    }
    decoded_samples = opus_decode( opus_data->dec,
                                   inframe->type==PJMEDIA_FRAME_TYPE_AUDIO ?
                                   inframe->buf : NULL,
                                   inframe->type==PJMEDIA_FRAME_TYPE_AUDIO ?
                                   inframe->size : 0,
                                   (opus_int16*)output->buf,
                                   frm_size,
                                   fec);
    output->timestamp = inframe->timestamp;
     
    if (inframe->type == PJMEDIA_FRAME_TYPE_AUDIO) {
        /* Mark current indexed frame as invalid */
        inframe->type = PJMEDIA_FRAME_TYPE_NONE;
        /* Update current frame index */
        opus_data->dec_frame_index++;
        if (opus_data->dec_frame_index > 1)
            opus_data->dec_frame_index = 0;
        /* Copy original input buffer to current indexed frame */
        inframe = &opus_data->dec_frame[opus_data->dec_frame_index];
        inframe->type = input->type;
        inframe->size = input->size;
        inframe->timestamp = input->timestamp;
        pj_memcpy(inframe->buf, input->buf, input->size);
    }

    if (decoded_samples < 0) {
        PJ_LOG(4, (THIS_FILE, "Decode failed!"));
        pj_mutex_unlock (opus_data->mutex);
        return PJMEDIA_CODEC_EFAILED;
    }

    output->size = decoded_samples * sizeof(opus_int16) * 
    		   opus_data->cfg.channel_cnt;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}


/*
 * Recover lost frame.
 */
static pj_status_t  codec_recover( pjmedia_codec *codec,
				   unsigned output_buf_len,
				   struct pjmedia_frame *output )
{
    struct opus_data *opus_data = (struct opus_data *)codec->codec_data;
    int decoded_samples;
    pjmedia_frame *inframe;
    int frm_size;

    PJ_UNUSED_ARG(output_buf_len);
    pj_mutex_lock (opus_data->mutex);

    if (opus_data->dec_frame_index == -1) {
        /* Recover the first packet? Don't think so, fill it with zeroes. */
	unsigned samples_per_frame;
	samples_per_frame = opus_data->cfg.sample_rate * opus_data->dec_ptime/
			    1000;
	output->type = PJMEDIA_FRAME_TYPE_AUDIO;
	output->size = samples_per_frame << 1;
	pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
        pj_mutex_unlock (opus_data->mutex);

        return PJ_SUCCESS;
    }

    inframe = &opus_data->dec_frame[opus_data->dec_frame_index];
    frm_size = output->size / (sizeof(opus_int16) *
               opus_data->cfg.channel_cnt);
    if (inframe->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	frm_size = PJ_MIN((unsigned)frm_size, opus_data->cfg.sample_rate *
			  opus_data->dec_ptime/1000);
    }
    decoded_samples = opus_decode(opus_data->dec,
				  inframe->type==PJMEDIA_FRAME_TYPE_AUDIO ?
				  inframe->buf : NULL,
				  inframe->type==PJMEDIA_FRAME_TYPE_AUDIO ?
				  inframe->size : 0,
				  (opus_int16*)output->buf,
				  frm_size,
				  0);

    /* Mark current indexed frame as invalid */
    inframe->type = PJMEDIA_FRAME_TYPE_NONE;
    
    /* Update current frame index */
    opus_data->dec_frame_index++;
    if (opus_data->dec_frame_index > 1)
        opus_data->dec_frame_index = 0;
    /* Mark current indexed frame as invalid */
    inframe = &opus_data->dec_frame[opus_data->dec_frame_index];
    inframe->type = PJMEDIA_FRAME_TYPE_NONE;

    if (decoded_samples < 0) {
        PJ_LOG(4, (THIS_FILE, "Recover failed!"));
        pj_mutex_unlock (opus_data->mutex);
        return PJMEDIA_CODEC_EFAILED;
    }

    output->size = decoded_samples * sizeof(opus_int16) * 
    		   opus_data->cfg.channel_cnt;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = inframe->timestamp;

    pj_mutex_unlock (opus_data->mutex);
    return PJ_SUCCESS;
}

#if defined(_MSC_VER)
#   pragma comment(lib, "libopus.a")
#endif


#endif /* PJMEDIA_HAS_OPUS_CODEC */

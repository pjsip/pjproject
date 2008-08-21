/* $Id$ */
/* 
 * Copyright (C)2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia-codec/ipp_codecs.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
 * Only build this file if PJMEDIA_HAS_INTEL_IPP != 0
 */
#if defined(PJMEDIA_HAS_INTEL_IPP) && PJMEDIA_HAS_INTEL_IPP != 0

#include <usc.h>

#define THIS_FILE   "ipp_codecs.c"

/* Prototypes for IPP codecs factory */
static pj_status_t ipp_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t ipp_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t ipp_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t ipp_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t ipp_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for IPP codecs implementation. */
static pj_status_t  ipp_codec_init( pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  ipp_codec_open( pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  ipp_codec_close( pjmedia_codec *codec );
static pj_status_t  ipp_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr );
static pj_status_t  ipp_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  ipp_codec_encode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  ipp_codec_decode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  ipp_codec_recover(pjmedia_codec *codec, 
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);

/* Definition for IPP codecs operations. */
static pjmedia_codec_op ipp_op = 
{
    &ipp_codec_init,
    &ipp_codec_open,
    &ipp_codec_close,
    &ipp_codec_modify,
    &ipp_codec_parse,
    &ipp_codec_encode,
    &ipp_codec_decode,
    &ipp_codec_recover
};

/* Definition for IPP codecs factory operations. */
static pjmedia_codec_factory_op ipp_factory_op =
{
    &ipp_test_alloc,
    &ipp_default_attr,
    &ipp_enum_codecs,
    &ipp_alloc_codec,
    &ipp_dealloc_codec
};

/* IPP codecs factory */
static struct ipp_factory {
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
} ipp_factory;

/* IPP codecs private data. */
typedef struct ipp_private {
    int			 codec_idx;	    /**< Codec index.		    */
    pj_pool_t		*pool;		    /**< Pool for each instance.    */

    USC_Handle		 enc;		    /**< Encoder state.		    */
    USC_Handle		 dec;		    /**< Decoder state.		    */
    USC_CodecInfo	*info;		    /**< Native codec info.	    */
    pj_uint16_t		 frame_size;	    /**< Bitstream frame size.	    */

    pj_bool_t		 plc_enabled;
    pjmedia_plc		*plc;

    pj_bool_t		 vad_enabled;
    pjmedia_silence_det	*vad;
    pj_timestamp	 last_tx;
} ipp_private_t;


/* USC codec implementations. */
extern USC_Fxns USC_G729AFP_Fxns;
extern USC_Fxns USC_G723_Fxns;
extern USC_Fxns USC_G726_Fxns;
extern USC_Fxns USC_G728_Fxns;
extern USC_Fxns USC_G722_Fxns;
extern USC_Fxns USC_GSMAMR_Fxns;
extern USC_Fxns USC_AMRWB_Fxns;
extern USC_Fxns USC_AMRWBE_Fxns;

/* CUSTOM CALLBACKS */

/* This callback is useful for translating RTP frame into USC frame, e.g:
 * reassigning frame attributes, reorder bitstream. Default behaviour of
 * the translation is just setting the USC frame buffer & its size as 
 * specified in RTP frame, setting USC frame frametype to 0, setting bitrate
 * of USC frame to bitrate info of codec_data. Implement this callback when 
 * the default behaviour is unapplicable.
 */
typedef void (*predecode_cb)(ipp_private_t *codec_data,
			     const pjmedia_frame *rtp_frame,
			     USC_Bitstream *usc_frame);

/* Parse frames from a packet. Default behaviour of frame parsing is 
 * just separating frames based on calculating frame length derived 
 * from bitrate. Implement this callback when the default behaviour is 
 * unapplicable.
 */
typedef pj_status_t (*parse_cb)(ipp_private_t *codec_data, void *pkt, 
				pj_size_t pkt_size, const pj_timestamp *ts,
				unsigned *frame_cnt, pjmedia_frame frames[]);

/* Pack frames into a packet. Default behaviour of packing frames is 
 * just stacking the frames with octet aligned without adding any 
 * payload header. Implement this callback when the default behaviour is
 * unapplicable.
 */
typedef pj_status_t (*pack_cb)(ipp_private_t *codec_data, void *pkt, 
			       pj_size_t *pkt_size, pj_size_t max_pkt_size);



/* Custom callback implementations. */
static    void predecode_g723( ipp_private_t *codec_data,
			       const pjmedia_frame *rtp_frame,
			       USC_Bitstream *usc_frame);
static pj_status_t parse_g723( ipp_private_t *codec_data, void *pkt, 
			       pj_size_t pkt_size, const pj_timestamp *ts,
			       unsigned *frame_cnt, pjmedia_frame frames[]);

static void predecode_g729( ipp_private_t *codec_data,
			    const pjmedia_frame *rtp_frame,
			    USC_Bitstream *usc_frame);

static    void predecode_amr( ipp_private_t *codec_data,
			      const pjmedia_frame *rtp_frame,
			      USC_Bitstream *usc_frame);
static pj_status_t parse_amr( ipp_private_t *codec_data, void *pkt, 
			      pj_size_t pkt_size, const pj_timestamp *ts,
			      unsigned *frame_cnt, pjmedia_frame frames[]);
static  pj_status_t pack_amr( ipp_private_t *codec_data, void *pkt, 
			      pj_size_t *pkt_size, pj_size_t max_pkt_size);


/* IPP codec implementation descriptions. */
static struct ipp_codec {
    int		     enabled;		/* Is this codec enabled?	    */
    const char	    *name;		/* Codec name.			    */
    pj_uint8_t	     pt;		/* Payload type.		    */
    USC_Fxns	    *fxns;		/* USC callback functions.	    */
    unsigned	     clock_rate;	/* Codec's clock rate.		    */
    unsigned	     channel_count;	/* Codec's channel count.	    */
    unsigned	     samples_per_frame;	/* Codec's samples count.	    */

    unsigned	     def_bitrate;	/* Default bitrate of this codec.   */
    unsigned	     max_bitrate;	/* Maximum bitrate of this codec.   */
    pj_uint8_t	     frm_per_pkt;	/* Default num of frames per packet.*/
    int		     has_native_vad;	/* Codec has internal VAD?	    */
    int		     has_native_plc;	/* Codec has internal PLC?	    */

    predecode_cb     predecode;		/* Callback to translate RTP frame
					   into USC frame		    */
    parse_cb	     parse;		/* Callback to parse bitstream	    */
    pack_cb	     pack;		/* Callback to pack bitstream	    */
} 

ipp_codec[] = 
{
#   if PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
    {1, "AMR",	    PJMEDIA_RTP_PT_AMR,       &USC_GSMAMR_Fxns,  8000, 1, 160, 
		    5900, 12200, 4, 1, 1, 
		    &predecode_amr, &parse_amr, &pack_amr
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB
    {1, "AMR-WB",   PJMEDIA_RTP_PT_AMRWB,     &USC_AMRWB_Fxns,  16000, 1, 320,
		    15850, 23850, 1, 1, 1, 
		    &predecode_amr, &parse_amr, &pack_amr
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_G729
    /* G.729 actually has internal VAD, but for now we need to disable it, 
     * since its RTP packaging (multiple frames per packet) requires 
     * SID frame to only be occured in the last frame, while controling 
     * encoder on each loop (to enable/disable VAD) is considered inefficient.
     * This should still be interoperable with other implementations.
     */
    {1, "G729",	    PJMEDIA_RTP_PT_G729,      &USC_G729AFP_Fxns, 8000, 1,  80,  
		    8000, 11800, 2, 0, 1, 
		    &predecode_g729, NULL, NULL
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1
    /* This is actually G.723.1 */
    {1, "G723",	    PJMEDIA_RTP_PT_G723,      &USC_G723_Fxns,	 8000, 1, 240,  
		    6300,  6300, 1, 1, 1, 
		    &predecode_g723, &parse_g723, NULL
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_G726
    {0, "G726-16",  PJMEDIA_RTP_PT_G726_16,   &USC_G726_Fxns,	 8000, 1,  80, 
		    16000, 16000, 2, 0, 0,
		    NULL, NULL, NULL
    },
    {0, "G726-24",  PJMEDIA_RTP_PT_G726_24,   &USC_G726_Fxns,	 8000, 1,  80, 
		    24000, 24000, 2, 0, 0,
		    NULL, NULL, NULL
    },
    {1, "G726-32",  PJMEDIA_RTP_PT_G726_32,   &USC_G726_Fxns,	 8000, 1,  80, 
		    32000, 32000, 2, 0, 0,
		    NULL, NULL, NULL
    },
    {0, "G726-40",  PJMEDIA_RTP_PT_G726_40,   &USC_G726_Fxns,	 8000, 1,  80, 
		    40000, 40000, 2, 0, 0,
		    NULL, NULL, NULL
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_G728
    {1, "G728",	    PJMEDIA_RTP_PT_G728,      &USC_G728_Fxns,	 8000, 1,  80, 
		    16000, 16000, 2, 0, 1,
		    NULL, NULL, NULL
    },
#   endif

#   if PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1
    {0, "G7221",    PJMEDIA_RTP_PT_G722_1,    &USC_G722_Fxns,	16000, 1, 320, 
		    16000, 32000, 1, 0, 1,
		    NULL, NULL, NULL
    },
#   endif
};


static int amr_get_mode(unsigned bitrate);

/*
 * Initialize and register IPP codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_ipp_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (ipp_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Create IPP codec factory. */
    ipp_factory.base.op = &ipp_factory_op;
    ipp_factory.base.factory_data = NULL;
    ipp_factory.endpt = endpt;

    ipp_factory.pool = pjmedia_endpt_create_pool(endpt, "IPP codecs", 4000, 4000);
    if (!ipp_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(ipp_factory.pool, "IPP codecs", 
				    &ipp_factory.mutex);
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
						&ipp_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(ipp_factory.pool);
    ipp_factory.pool = NULL;
    return status;
}

/*
 * Unregister IPP codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_ipp_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (ipp_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    pj_mutex_lock(ipp_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(ipp_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(ipp_factory.pool);
	ipp_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister IPP codecs factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &ipp_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(ipp_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(ipp_factory.pool);
    ipp_factory.pool = NULL;

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t ipp_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    for (i = 0; i < PJ_ARRAY_SIZE(ipp_codec); ++i) {
	pj_str_t name = pj_str((char*)ipp_codec[i].name);
	if ((pj_stricmp(&info->encoding_name, &name) == 0) &&
	    (info->clock_rate == (unsigned)ipp_codec[i].clock_rate) &&
	    (info->channel_cnt == (unsigned)ipp_codec[i].channel_count) &&
	    (ipp_codec[i].enabled))
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
static pj_status_t ipp_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory==&ipp_factory.base, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_codec_param));

    for (i = 0; i < PJ_ARRAY_SIZE(ipp_codec); ++i) {
	pj_str_t name = pj_str((char*)ipp_codec[i].name);
	if ((pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)ipp_codec[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)ipp_codec[i].channel_count))
	{
	    attr->info.pt = (pj_uint8_t)id->pt;
	    attr->info.channel_cnt = ipp_codec[i].channel_count;
	    attr->info.clock_rate = ipp_codec[i].clock_rate;
	    attr->info.avg_bps = ipp_codec[i].def_bitrate;
	    attr->info.max_bps = ipp_codec[i].max_bitrate;
	    attr->info.pcm_bits_per_sample = 16;
	    attr->info.frm_ptime =  (pj_uint16_t)
				    (ipp_codec[i].samples_per_frame * 1000 / 
				    ipp_codec[i].channel_count / 
				    ipp_codec[i].clock_rate);
	    attr->setting.frm_per_pkt = ipp_codec[i].frm_per_pkt;

	    /* Default flags. */
	    attr->setting.cng = 0;
	    attr->setting.plc = 1;
	    attr->setting.penh= 0;
	    attr->setting.vad = 1; /* Always disable for now */

	    return PJ_SUCCESS;
	}
    }

    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t ipp_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    unsigned max;
    unsigned i;

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;
    
    for (i = 0, *count = 0; i < PJ_ARRAY_SIZE(ipp_codec) && *count < max; ++i) 
    {
	if (!ipp_codec[i].enabled)
	    continue;

	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str((char*)ipp_codec[i].name);
	codecs[*count].pt = ipp_codec[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = ipp_codec[i].clock_rate;
	codecs[*count].channel_cnt = ipp_codec[i].channel_count;

	++*count;
    }

    return PJ_SUCCESS;
}

/*
 * Allocate a new codec instance.
 */
static pj_status_t ipp_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    ipp_private_t *codec_data;
    pjmedia_codec *codec;
    int idx;
    pj_pool_t *pool;
    unsigned i;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ipp_factory.base, PJ_EINVAL);

    pj_mutex_lock(ipp_factory.mutex);

    /* Find codec's index */
    idx = -1;
    for (i = 0; i < PJ_ARRAY_SIZE(ipp_codec); ++i) {
	pj_str_t name = pj_str((char*)ipp_codec[i].name);
	if ((pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)ipp_codec[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)ipp_codec[i].channel_count) &&
	    (ipp_codec[i].enabled))
	{
	    idx = i;
	    break;
	}
    }
    if (idx == -1) {
	*p_codec = NULL;
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(ipp_factory.endpt, "IPPcodec", 512, 512);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
    codec->op = &ipp_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, ipp_private_t);
    codec_data = (ipp_private_t*) codec->codec_data;

    /* Create PLC if codec has no internal PLC */
    if (!ipp_codec[idx].has_native_plc) {
	pj_status_t status;
	status = pjmedia_plc_create(pool, ipp_codec[idx].clock_rate, 
				    ipp_codec[idx].samples_per_frame, 0,
				    &codec_data->plc);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(pool);
	    pj_mutex_unlock(ipp_factory.mutex);
	    return status;
	}
    }

    /* Create silence detector if codec has no internal VAD */
    if (!ipp_codec[idx].has_native_vad) {
	pj_status_t status;
	status = pjmedia_silence_det_create(pool,
					    ipp_codec[idx].clock_rate,
					    ipp_codec[idx].samples_per_frame,
					    &codec_data->vad);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(pool);
	    pj_mutex_unlock(ipp_factory.mutex);
	    return status;
	}
    }

    codec_data->pool = pool;
    codec_data->codec_idx = idx;

    pj_mutex_unlock(ipp_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t ipp_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    ipp_private_t *codec_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ipp_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    codec_data = (ipp_private_t*) codec->codec_data;
    if (codec_data->enc != NULL || codec_data->dec != NULL) {
	ipp_codec_close(codec);
    }

    pj_pool_release(codec_data->pool);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t ipp_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t ipp_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];
    int info_size;
    pj_pool_t *pool;
    int i, j;
    USC_MemBank *membanks;
    int nb_membanks;

    pool = codec_data->pool;

    /* Get the codec info size */
    if (USC_NoError != ippc->fxns->std.GetInfoSize(&info_size)) {
	PJ_LOG(1,(THIS_FILE, "Error getting codec info size"));
	goto on_error;
    }
    /* Get the codec info */
    codec_data->info = pj_pool_zalloc(pool, info_size);
    if (USC_NoError != ippc->fxns->std.GetInfo((USC_Handle)NULL, 
					       codec_data->info))
    {
	PJ_LOG(1,(THIS_FILE, "Error getting codec info"));
	goto on_error;
    }

    /* PREPARING THE ENCODER */

    /* Setting the encoder params */
    codec_data->info->params.direction = USC_ENCODE;
    codec_data->info->params.modes.vad = attr->setting.vad && 
					   ippc->has_native_vad;
    codec_data->info->params.modes.bitrate = attr->info.avg_bps;
    codec_data->info->params.law = 0; /* Linear PCM input */

    /* Get number of memory blocks needed by the encoder */
    if (USC_NoError != ippc->fxns->std.NumAlloc(&codec_data->info->params,
					        &nb_membanks))
    {
	PJ_LOG(1,(THIS_FILE, "Error getting no of memory blocks of encoder"));
	goto on_error;
    }

    /* Allocate memory blocks table */
    membanks = (USC_MemBank*) pj_pool_zalloc(pool, 
					     sizeof(USC_MemBank) * nb_membanks);
    /* Get size of each memory block */
    if (USC_NoError != ippc->fxns->std.MemAlloc(&codec_data->info->params, 
					        membanks))
    {
	PJ_LOG(1,(THIS_FILE, "Error getting memory blocks size of encoder"));
	goto on_error;
    }

    /* Allocate memory for each block */
    for (i = 0; i < nb_membanks; i++) {
	membanks[i].pMem = (char*) pj_pool_zalloc(pool, membanks[i].nbytes);
    }

    /* Create encoder instance */
    if (USC_NoError != ippc->fxns->std.Init(&codec_data->info->params,
					    membanks, 
					    &codec_data->enc))
    {
	PJ_LOG(1,(THIS_FILE, "Error initializing encoder"));
	goto on_error;
    }

    /* PREPARING THE DECODER */

    /* Setting the decoder params */
    codec_data->info->params.direction = USC_DECODE;

    /* Get number of memory blocks needed by the decoder */
    if (USC_NoError != ippc->fxns->std.NumAlloc(&codec_data->info->params, 
						 &nb_membanks))
    {
	PJ_LOG(1,(THIS_FILE, "Error getting no of memory blocks of decoder"));
	goto on_error;
    }

    /* Allocate memory blocks table */
    membanks = (USC_MemBank*) pj_pool_zalloc(pool, 
					     sizeof(USC_MemBank) * nb_membanks);
    /* Get size of each memory block */
    if (USC_NoError != ippc->fxns->std.MemAlloc(&codec_data->info->params, 
						membanks))
    {
	PJ_LOG(1,(THIS_FILE, "Error getting memory blocks size of decoder"));
	goto on_error;
    }

    /* Allocate memory for each block */
    for (i = 0; i < nb_membanks; i++) {
	membanks[i].pMem = (char*) pj_pool_zalloc(pool, membanks[i].nbytes);
    }

    /* Create decoder instance */
    if (USC_NoError != ippc->fxns->std.Init(&codec_data->info->params, 
					    membanks, &codec_data->dec))
    {
	PJ_LOG(1,(THIS_FILE, "Error initializing decoder"));
	goto on_error;
    }

    /* Update codec info */
    ippc->fxns->std.GetInfo((USC_Handle)codec_data->enc, codec_data->info);

    /* Get bitstream size */
    i = codec_data->info->params.modes.bitrate * ippc->samples_per_frame;
    j = ippc->clock_rate << 3;
    codec_data->frame_size = (pj_uint16_t)(i / j);
    if (i % j) ++codec_data->frame_size;

    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);

    return PJ_SUCCESS;

on_error:
    return PJMEDIA_CODEC_EFAILED;
}

/*
 * Close codec.
 */
static pj_status_t ipp_codec_close( pjmedia_codec *codec )
{
    PJ_UNUSED_ARG(codec);

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  ipp_codec_modify(pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];

    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);

    if (ippc->has_native_vad) {
	USC_Modes modes;

	modes = codec_data->info->params.modes;
	modes.vad = codec_data->vad_enabled;
	ippc->fxns->std.Control(&modes, codec_data->enc);
    }

    return PJ_SUCCESS;
}

/*
 * Get frames in the packet.
 */
static pj_status_t  ipp_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[])
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];
    unsigned count = 0;

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    if (ippc->parse != NULL) {
	return ippc->parse(codec_data, pkt,  pkt_size, ts, frame_cnt, frames);
    }

    while (pkt_size >= codec_data->frame_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = codec_data->frame_size;
	frames[count].timestamp.u64 = ts->u64 + count*ippc->samples_per_frame;

	pkt = ((char*)pkt) + codec_data->frame_size;
	pkt_size -= codec_data->frame_size;

	++count;
    }

    if (pkt_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = pkt_size;
	frames[count].timestamp.u64 = ts->u64 + count*ippc->samples_per_frame;
	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frames.
 */
static pj_status_t ipp_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];
    unsigned samples_per_frame;
    unsigned nsamples;
    pj_size_t tx = 0;
    pj_int16_t *pcm_in   = (pj_int16_t*)input->buf;
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
	    PJMEDIA_CODEC_MAX_SILENCE_PERIOD != -1 &&
	    silence_duration < (PJMEDIA_CODEC_MAX_SILENCE_PERIOD *
				(int)ippc->clock_rate / 1000)) 
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
    samples_per_frame = ippc->samples_per_frame;
    pt = ippc->pt;

    PJ_ASSERT_RETURN(nsamples % samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    /* Encode the frames */
    while (nsamples >= samples_per_frame) {
	USC_PCMStream in;
	USC_Bitstream out;

	in.bitrate = codec_data->info->params.modes.bitrate;
	in.nbytes = samples_per_frame << 1;
	in.pBuffer = (char*)pcm_in;
	in.pcmType.bitPerSample = codec_data->info->params.pcmType.bitPerSample;
	in.pcmType.nChannels = codec_data->info->params.pcmType.nChannels;
	in.pcmType.sample_frequency = codec_data->info->params.pcmType.sample_frequency;

	out.pBuffer = (char*)bits_out;

#if PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
	/* For AMR: reserve the first byte for frame info */
	if (pt == PJMEDIA_RTP_PT_AMR || pt == PJMEDIA_RTP_PT_AMRWB) {
	    ++out.pBuffer;
	}
#endif

	if (USC_NoError != ippc->fxns->Encode(codec_data->enc, &in, &out)) {
	    break;
	}

#if PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
	/* For AMR: put info (frametype, degraded, last frame) in the 
	 * first byte 
	 */
	if (pt == PJMEDIA_RTP_PT_AMR || pt == PJMEDIA_RTP_PT_AMRWB) {
	    pj_uint8_t *info = (pj_uint8_t*)bits_out;

	    ++out.nbytes;

	    /* One byte AMR frame type & quality flag:
	     * bit 0-3	: frame type
	     * bit 6	: last frame flag
	     * bit 7	: quality flag
	     */
	    if (out.frametype == 0 || out.frametype == 4 || 
		(pt == PJMEDIA_RTP_PT_AMR && out.frametype == 5) ||
		(pt == PJMEDIA_RTP_PT_AMRWB && out.frametype == 6))
	    {
		/* Speech */
		*info = (char)amr_get_mode(out.bitrate);
		/* Degraded */
		if (out.frametype == 5 || out.frametype == 6)
		    *info |= 0x80;
	    } else if (out.frametype == 1 || out.frametype == 2 || 
		       (pt == PJMEDIA_RTP_PT_AMR && out.frametype == 6) ||
		       (pt == PJMEDIA_RTP_PT_AMRWB && out.frametype == 7))
	    {
		/* SID */
		*info = (pj_uint8_t)(pt == PJMEDIA_RTP_PT_AMRWB? 9 : 8);
		/* Degraded */
		if (out.frametype == 6 || out.frametype == 7)
		    *info |= 0x80;
	    } else {
		/* Untransmited */
		*info = 15;
		out.nbytes = 1;
	    }

	    /* Last frame flag */
	    if (nsamples == samples_per_frame)
		*info |= 0x40;
	}
#endif

	pcm_in += samples_per_frame;
	nsamples -= samples_per_frame;
	tx += out.nbytes;
	bits_out += out.nbytes;
    }

    if (ippc->pack != NULL) {
	ippc->pack(codec_data, output->buf, &tx, output_buf_len);
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
}

/*
 * Decode frame.
 */
static pj_status_t ipp_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];
    unsigned samples_per_frame;
    USC_PCMStream out;
    USC_Bitstream in;
    pj_uint8_t pt;

    pt = ippc->pt; 
    samples_per_frame = ippc->samples_per_frame;

    PJ_ASSERT_RETURN(output_buf_len >= samples_per_frame << 1,
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    if (input->type == PJMEDIA_FRAME_TYPE_AUDIO) {
	if (ippc->predecode) {
	    ippc->predecode(codec_data, input, &in);
	} else {
	    /* Most IPP codecs have frametype==0 for speech frame */
	    in.pBuffer = (char*)input->buf;
	    in.nbytes = input->size;
	    in.frametype = 0;
	    in.bitrate = codec_data->info->params.modes.bitrate;
	}

	out.pBuffer = output->buf;
    }

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO ||
	USC_NoError != ippc->fxns->Decode(codec_data->dec, &in, &out)) 
    {
	pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
	output->size = samples_per_frame << 1;
	output->timestamp.u64 = input->timestamp.u64;
	output->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return PJ_SUCCESS;
    }

#if PJMEDIA_HAS_INTEL_IPP_CODEC_G726
    /* For G.726: amplify decoding result (USC G.726 encoder deamplified it) */
    if (pt == PJMEDIA_RTP_PT_G726_16 || pt == PJMEDIA_RTP_PT_G726_24 ||
	pt == PJMEDIA_RTP_PT_G726_32 || pt == PJMEDIA_RTP_PT_G726_40)
    {
	unsigned i;
	pj_int16_t *s = (pj_int16_t*)output->buf;

	for (i = 0; i < samples_per_frame; ++i)
	    s[i] <<= 2;
    }
#endif

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = samples_per_frame << 1;
    output->timestamp.u64 = input->timestamp.u64;

    /* Invoke external PLC if codec has no internal PLC */
    if (codec_data->plc && codec_data->plc_enabled)
	pjmedia_plc_save(codec_data->plc, (pj_int16_t*)output->buf);

    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t  ipp_codec_recover(pjmedia_codec *codec, 
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output)
{
    ipp_private_t *codec_data = (ipp_private_t*) codec->codec_data;
    struct ipp_codec *ippc = &ipp_codec[codec_data->codec_idx];
    unsigned samples_per_frame;

    PJ_UNUSED_ARG(output_buf_len);

    samples_per_frame = ippc->samples_per_frame;

    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = samples_per_frame << 1;

    if (codec_data->plc_enabled) {
	if (codec_data->plc) {
	    pjmedia_plc_generate(codec_data->plc, (pj_int16_t*)output->buf);
	} else {
	    USC_PCMStream out;
	    out.pBuffer = output->buf;
	    ippc->fxns->Decode(codec_data->dec, NULL, &out);
	}
    } else {
	pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
    }

    return PJ_SUCCESS;
}

#if PJMEDIA_HAS_INTEL_IPP_CODEC_G729

static void predecode_g729( ipp_private_t *codec_data,
			    const pjmedia_frame *rtp_frame,
			    USC_Bitstream *usc_frame)
{
    switch (rtp_frame->size) {
    case 2:
	/* SID */
	usc_frame->frametype = 1;
	usc_frame->bitrate = codec_data->info->params.modes.bitrate;
	break;
    case 8:  
	/* G729D */
	usc_frame->frametype = 2;
	usc_frame->bitrate = 6400;
	break;
    case 10: 
	/* G729 */
	usc_frame->frametype = 3;
	usc_frame->bitrate = 8000;
	break;
    case 15: 
	/* G729E */
	usc_frame->frametype = 4;
	usc_frame->bitrate = 11800;
	break;
    default: 
	usc_frame->frametype = 0;
	usc_frame->bitrate = 0;
	break;
    }

    usc_frame->pBuffer = rtp_frame->buf;
    usc_frame->nbytes = rtp_frame->size;
}

#endif /* PJMEDIA_HAS_INTEL_IPP_CODEC_G729 */


#if PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1

static    void predecode_g723( ipp_private_t *codec_data,
			       const pjmedia_frame *rtp_frame,
			       USC_Bitstream *usc_frame)
{
    int i, HDR = 0;
    pj_uint8_t *f = (pj_uint8_t*)rtp_frame->buf;

    PJ_UNUSED_ARG(codec_data);

    for (i = 0; i < 2; ++i){
	int tmp;
	tmp = (f[0] >> (i & 0x7)) & 1;
	HDR +=  tmp << i ;
    }

    usc_frame->pBuffer = rtp_frame->buf;
    usc_frame->nbytes = rtp_frame->size;
    usc_frame->bitrate = HDR == 0? 6300 : 5300;
    usc_frame->frametype = 0;
}

static pj_status_t parse_g723(ipp_private_t *codec_data, void *pkt, 
			      pj_size_t pkt_size, const pj_timestamp *ts,
			      unsigned *frame_cnt, pjmedia_frame frames[])
{
    unsigned count = 0;
    pj_uint8_t *f = (pj_uint8_t*)pkt;

    while (pkt_size && count < *frame_cnt) {
	int framesize, i, j;
	int HDR = 0;

	for (i = 0; i < 2; ++i){
	    j = (f[0] >> (i & 0x7)) & 1;
	    HDR +=  j << i ;
	}

	if (HDR == 0)
	    framesize = 24;
	else if (HDR == 1)
	    framesize = 20;
	else if (HDR == 2)
	    framesize = 4;
	else if (HDR == 3)
	    framesize = 1;
	else {
	    pj_assert(!"Unknown G723.1 frametype, packet may be corrupted!");
	    return PJMEDIA_CODEC_EINMODE;
	}

	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = f;
	frames[count].size = framesize;
	frames[count].timestamp.u64 = ts->u64 + count * 
			ipp_codec[codec_data->codec_idx].samples_per_frame;

	f += framesize;
	pkt_size -= framesize;

	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1 */


#if PJMEDIA_HAS_INTEL_IPP_CODEC_AMR

/* AMR bitstream sensitivity order map */
static pj_int16_t AMRNB_ordermap122[244] =
{
    0,    1,     2,    3,    4,    5,    6,    7,    8,    9,
    10,   11,   12,   13,   14,   23,   15,   16,   17,   18,
    19,   20,   21,   22,   24,   25,   26,   27,   28,   38,
    141,  39,  142,   40,  143,   41,  144,   42,  145,   43,
    146,  44,  147,   45,  148,   46,  149,   47,   97,  150,
    200,  48,   98,  151,  201,   49,   99,  152,  202,   86,
    136, 189,  239,   87,  137,  190,  240,   88,  138,  191,
    241,  91,  194,   92,  195,   93,  196,   94,  197,   95,
    198,  29,   30,   31,   32,   33,   34,   35,   50,  100,
    153, 203,   89,  139,  192,  242,   51,  101,  154,  204,
    55,  105,  158,  208,   90,  140,  193,  243,   59,  109,
    162, 212,   63,  113,  166,  216,   67,  117,  170,  220,
    36,   37,   54,   53,   52,   58,   57,   56,   62,   61,
    60,   66,   65,   64,   70,   69,   68,  104,  103,  102,
    108, 107,  106,  112,  111,  110,  116,  115,  114,  120,
    119, 118,  157,  156,  155,  161,  160,  159,  165,  164,
    163, 169,  168,  167,  173,  172,  171,  207,  206,  205,
    211, 210,  209,  215,  214,  213,  219,  218,  217,  223,
    222, 221,   73,   72,   71,   76,   75,   74,   79,   78,
    77,   82,   81,   80,   85,   84,   83,  123,  122,  121,
    126, 125,  124,  129,  128,  127,  132,  131,  130,  135,
    134, 133,  176,  175,  174,  179,  178,  177,  182,  181,
    180, 185,  184,  183,  188,  187,  186,  226,  225,  224,
    229, 228,  227,  232,  231,  230,  235,  234,  233,  238,
    237, 236,   96,  199
};

static pj_int16_t AMRNB_ordermap102[204] =
{
    7,     6,  5,    4,  3,    2,   1,   0,  16,  15,
    14,   13,  12,  11,  10,   9,   8,  26,  27,  28,
    29,   30,  31, 115, 116, 117, 118, 119, 120,  72,
    73,  161, 162,  65,  68,  69, 108, 111, 112, 154,
    157, 158, 197, 200, 201,  32,  33, 121, 122,  74,
    75,  163, 164,  66, 109, 155, 198,  19,  23,  21,
    22,   18,  17,  20,  24,  25,  37,  36,  35,  34,
    80,   79,  78,  77, 126, 125, 124, 123, 169, 168,
    167, 166,  70,  67,  71, 113, 110, 114, 159, 156,
    160, 202, 199, 203,  76, 165,  81,  82,  92,  91,
    93,   83,  95,  85,  84,  94, 101, 102,  96, 104,
    86,  103,  87,  97, 127, 128, 138, 137, 139, 129,
    141, 131, 130, 140, 147, 148, 142, 150, 132, 149,
    133, 143, 170, 171, 181, 180, 182, 172, 184, 174,
    173, 183, 190, 191, 185, 193, 175, 192, 176, 186,
    38,   39,  49,  48,  50,  40,  52,  42,  41,  51,
    58,   59,  53,  61,  43,  60,  44,  54, 194, 179,
    189, 196, 177, 195, 178, 187, 188, 151, 136, 146,
    153, 134, 152, 135, 144, 145, 105,  90, 100, 107,
    88,  106,  89,  98,  99,  62,  47,  57,  64,  45,
    63,   46,  55,  56
};

static pj_int16_t AMRNB_ordermap795[159] =
{
    8,    7,    6,    5,    4,    3,    2,   14,   16,    9,
    10,   12,   13,   15,   11,   17,   20,   22,   24,   23,
    19,   18,   21,   56,   88,  122,  154,   57,   89,  123,
    155,  58,   90,  124,  156,   52,   84,  118,  150,   53,
    85,  119,  151,   27,   93,   28,   94,   29,   95,   30,
    96,   31,   97,   61,  127,   62,  128,   63,  129,   59,
    91,  125,  157,   32,   98,   64,  130,    1,    0,   25,
    26,   33,   99,   34,  100,   65,  131,   66,  132,   54,
    86,  120,  152,   60,   92,  126,  158,   55,   87,  121,
    153, 117,  116,  115,   46,   78,  112,  144,   43,   75,
    109, 141,   40,   72,  106,  138,   36,   68,  102,  134,
    114, 149,  148,  147,  146,   83,   82,   81,   80,   51,
    50,   49,   48,   47,   45,   44,   42,   39,   35,   79,
    77,   76,   74,   71,   67,  113,  111,  110,  108,  105,
    101, 145,  143,  142,  140,  137,  133,   41,   73,  107,
    139,  37,   69,  103,  135,   38,   70,  104,  136

};

static pj_int16_t AMRNB_ordermap74[148] =
{
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
     10,  11,  12,  13,  14,  15,  16,  26,  87,  27,
     88,  28,  89,  29,  90,  30,  91,  51,  80, 112,
    141,  52,  81, 113, 142,  54,  83, 115, 144,  55,
     84, 116, 145,  58, 119,  59, 120,  21,  22,  23,
     17,  18,  19,  31,  60,  92, 121,  56,  85, 117,
    146,  20,  24,  25,  50,  79, 111, 140,  57,  86,
    118, 147,  49,  78, 110, 139,  48,  77,  53,  82,
    114, 143, 109, 138,  47,  76, 108, 137,  32,  33,
     61,  62,  93,  94, 122, 123,  41,  42,  43,  44,
     45,  46,  70,  71,  72,  73,  74,  75, 102, 103,
    104, 105, 106, 107, 131, 132, 133, 134, 135, 136,
     34,  63,  95, 124,  35,  64,  96, 125,  36,  65,
     97, 126,  37,  66,  98, 127,  38,  67,  99, 128,
     39,  68, 100, 129,  40,  69, 101, 130
};

static pj_int16_t AMRNB_ordermap67[134] =
{
      0,   1,    4,    3,    5,    6,   13,    7,    2,    8,
      9,  11,   15,   12,   14,   10,   28,   82,   29,   83,
     27,  81,   26,   80,   30,   84,   16,   55,  109,   56,
    110,  31,   85,   57,  111,   48,   73,  102,  127,   32,
     86,  51,   76,  105,  130,   52,   77,  106,  131,   58,
    112,  33,   87,   19,   23,   53,   78,  107,  132,   21,
     22,  18,   17,   20,   24,   25,   50,   75,  104,  129,
     47,  72,  101,  126,   54,   79,  108,  133,   46,   71,
    100, 125,  128,  103,   74,   49,   45,   70,   99,  124,
     42,  67,   96,  121,   39,   64,   93,  118,   38,   63,
     92, 117,   35,   60,   89,  114,   34,   59,   88,  113,
     44,  69,   98,  123,   43,   68,   97,  122,   41,   66,
     95, 120,   40,   65,   94,  119,   37,   62,   91,  116,
     36,  61,   90,  115
};

static pj_int16_t AMRNB_ordermap59[118] =
{
    0,     1,    4,    5,    3,    6,    7,    2,   13,   15,
    8,     9,   11,   12,   14,   10,   16,   28,   74,   29,
    75,   27,   73,   26,   72,   30,   76,   51,   97,   50,
    71,   96,  117,   31,   77,   52,   98,   49,   70,   95,
    116,  53,   99,   32,   78,   33,   79,   48,   69,   94,
    115,  47,   68,   93,  114,   46,   67,   92,  113,   19,
    21,   23,   22,   18,   17,   20,   24,  111,   43,   89,
    110,  64,   65,   44,   90,   25,   45,   66,   91,  112,
    54,  100,   40,   61,   86,  107,   39,   60,   85,  106,
    36,   57,   82,  103,   35,   56,   81,  102,   34,   55,
    80,  101,   42,   63,   88,  109,   41,   62,   87,  108,
    38,   59,   84,  105,   37,   58,   83,  104
};

static pj_int16_t AMRNB_ordermap515[103] =
{
     7,    6,    5,    4,    3,    2,    1,    0,   15,   14,
    13,   12,   11,   10,    9,    8,   23,   24,   25,   26,
    27,   46,   65,   84,   45,   44,   43,   64,   63,   62,
    83,   82,   81,  102,  101,  100,   42,   61,   80,   99,
    28,   47,   66,   85,   18,   41,   60,   79,   98,   29,
    48,   67,   17,   20,   22,   40,   59,   78,   97,   21,
    30,   49,   68,   86,   19,   16,   87,   39,   38,   58,
    57,   77,   35,   54,   73,   92,   76,   96,   95,   36,
    55,   74,   93,   32,   51,   33,   52,   70,   71,   89,
    90,   31,   50,   69,   88,   37,   56,   75,   94,   34,
    53,   72,   91
};

static pj_int16_t AMRNB_ordermap475[95] =
{
     0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  23,  24,  25,  26,
    27,  28,  48,  49,  61,  62,  82,  83,  47,  46,
    45,  44,  81,  80,  79,  78,  17,  18,  20,  22,
    77,  76,  75,  74,  29,  30,  43,  42,  41,  40,
    38,  39,  16,  19,  21,  50,  51,  59,  60,  63,
    64,  72,  73,  84,  85,  93,  94,  32,  33,  35,
    36,  53,  54,  56,  57,  66,  67,  69,  70,  87,
    88,  90,  91,  34,  55,  68,  89,  37,  58,  71,
    92,  31,  52,  65,  86
};

static pj_int16_t *AMRNB_ordermaps[8] =
{
    AMRNB_ordermap475,
    AMRNB_ordermap515,
    AMRNB_ordermap59,
    AMRNB_ordermap67,
    AMRNB_ordermap74,
    AMRNB_ordermap795,
    AMRNB_ordermap102,
    AMRNB_ordermap122
};

static pj_int16_t AMRWB_ordermap_660[] =
{
      0,   5,   6,   7,  61,  84, 107, 130,  62,  85,
      8,   4,  37,  38,  39,  40,  58,  81, 104, 127,
     60,  83, 106, 129, 108, 131, 128,  41,  42,  80,
    126,   1,   3,  57, 103,  82, 105,  59,   2,  63,
    109, 110,  86,  19,  22,  23,  64,  87,  18,  20,
     21,  17,  13,  88,  43,  89,  65, 111,  14,  24,
     25,  26,  27,  28,  15,  16,  44,  90,  66, 112,
      9,  11,  10,  12,  67, 113,  29,  30,  31,  32,
     34,  33,  35,  36,  45,  51,  68,  74,  91,  97,
    114, 120,  46,  69,  92, 115,  52,  75,  98, 121,
     47,  70,  93, 116,  53,  76,  99, 122,  48,  71,
     94, 117,  54,  77, 100, 123,  49,  72,  95, 118,
     55,  78, 101, 124,  50,  73,  96, 119,  56,  79,
    102, 125
};

static pj_int16_t AMRWB_ordermap_885[] =
{
      0,   4,   6,   7,   5,   3,  47,  48,  49, 112,
    113, 114,  75, 106, 140, 171,  80, 111, 145, 176,
     77, 108, 142, 173,  78, 109, 143, 174,  79, 110,
    144, 175,  76, 107, 141, 172,  50, 115,  51,   2,
      1,  81, 116, 146,  19,  21,  12,  17,  18,  20,
     16,  25,  13,  10,  14,  24,  23,  22,  26,   8,
     15,  52, 117,  31,  82, 147,   9,  33,  11,  83,
    148,  53, 118,  28,  27,  84, 149,  34,  35,  29,
     46,  32,  30,  54, 119,  37,  36,  39,  38,  40,
     85, 150,  41,  42,  43,  44,  45,  55,  60,  65,
     70,  86,  91,  96, 101, 120, 125, 130, 135, 151,
    156, 161, 166,  56,  87, 121, 152,  61,  92, 126,
    157,  66,  97, 131, 162,  71, 102, 136, 167,  57,
     88, 122, 153,  62,  93, 127, 158,  67,  98, 132,
    163,  72, 103, 137, 168,  58,  89, 123, 154,  63,
     94, 128, 159,  68,  99, 133, 164,  73, 104, 138,
    169,  59,  90, 124, 155,  64,  95, 129, 160,  69,
    100, 134, 165,  74, 105, 139, 170
};

static pj_int16_t AMRWB_ordermap_1265[] =
{
      0,   4,   6,  93, 143, 196, 246,   7,   5,   3,
     47,  48,  49,  50,  51, 150, 151, 152, 153, 154,
     94, 144, 197, 247,  99, 149, 202, 252,  96, 146,
    199, 249,  97, 147, 200, 250, 100, 203,  98, 148,
    201, 251,  95, 145, 198, 248,  52,   2,   1, 101,
    204, 155,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    156,  31, 102, 205,   9,  33,  11, 103, 206,  54,
    157,  28,  27, 104, 207,  34,  35,  29,  46,  32,
     30,  55, 158,  37,  36,  39,  38,  40, 105, 208,
     41,  42,  43,  44,  45,  56, 106, 159, 209,  57,
     66,  75,  84, 107, 116, 125, 134, 160, 169, 178,
    187, 210, 219, 228, 237,  58, 108, 161, 211,  62,
    112, 165, 215,  67, 117, 170, 220,  71, 121, 174,
    224,  76, 126, 179, 229,  80, 130, 183, 233,  85,
    135, 188, 238,  89, 139, 192, 242,  59, 109, 162,
    212,  63, 113, 166, 216,  68, 118, 171, 221,  72,
    122, 175, 225,  77, 127, 180, 230,  81, 131, 184,
    234,  86, 136, 189, 239,  90, 140, 193, 243,  60,
    110, 163, 213,  64, 114, 167, 217,  69, 119, 172,
    222,  73, 123, 176, 226,  78, 128, 181, 231,  82,
    132, 185, 235,  87, 137, 190, 240,  91, 141, 194,
    244,  61, 111, 164, 214,  65, 115, 168, 218,  70,
    120, 173, 223,  74, 124, 177, 227,  79, 129, 182,
    232,  83, 133, 186, 236,  88, 138, 191, 241,  92,
    142, 195, 245
};

static pj_int16_t AMRWB_ordermap_1425[] =
{
      0,   4,   6, 101, 159, 220, 278,   7,   5,   3,
     47,  48,  49,  50,  51, 166, 167, 168, 169, 170,
    102, 160, 221, 279, 107, 165, 226, 284, 104, 162,
    223, 281, 105, 163, 224, 282, 108, 227, 106, 164,
    225, 283, 103, 161, 222, 280,  52,   2,   1, 109,
    228, 171,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    172,  31, 110, 229,   9,  33,  11, 111, 230,  54,
    173,  28,  27, 112, 231,  34,  35,  29,  46,  32,
     30,  55, 174,  37,  36,  39,  38,  40, 113, 232,
     41,  42,  43,  44,  45,  56, 114, 175, 233,  62,
    120, 181, 239,  75, 133, 194, 252,  57, 115, 176,
    234,  63, 121, 182, 240,  70, 128, 189, 247,  76,
    134, 195, 253,  83, 141, 202, 260,  92, 150, 211,
    269,  84, 142, 203, 261,  93, 151, 212, 270,  85,
    143, 204, 262,  94, 152, 213, 271,  86, 144, 205,
    263,  95, 153, 214, 272,  64, 122, 183, 241,  77,
    135, 196, 254,  65, 123, 184, 242,  78, 136, 197,
    255,  87, 145, 206, 264,  96, 154, 215, 273,  58,
    116, 177, 235,  66, 124, 185, 243,  71, 129, 190,
    248,  79, 137, 198, 256,  88, 146, 207, 265,  97,
    155, 216, 274,  59, 117, 178, 236,  67, 125, 186,
    244,  72, 130, 191, 249,  80, 138, 199, 257,  89,
    147, 208, 266,  98, 156, 217, 275,  60, 118, 179,
    237,  68, 126, 187, 245,  73, 131, 192, 250,  81,
    139, 200, 258,  90, 148, 209, 267,  99, 157, 218,
    276,  61, 119, 180, 238,  69, 127, 188, 246,  74,
    132, 193, 251,  82, 140, 201, 259,  91, 149, 210,
    268, 100, 158, 219, 277
};

static pj_int16_t AMRWB_ordermap_1585[] =
{
      0,   4,   6, 109, 175, 244, 310,   7,   5,   3,
     47,  48,  49,  50,  51, 182, 183, 184, 185, 186,
    110, 176, 245, 311, 115, 181, 250, 316, 112, 178,
    247, 313, 113, 179, 248, 314, 116, 251, 114, 180,
    249, 315, 111, 177, 246, 312,  52,   2,   1, 117,
    252, 187,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    188,  31, 118, 253,   9,  33,  11, 119, 254,  54,
    189,  28,  27, 120, 255,  34,  35,  29,  46,  32,
     30,  55, 190,  37,  36,  39,  38,  40, 121, 256,
     41,  42,  43,  44,  45,  56, 122, 191, 257,  63,
    129, 198, 264,  76, 142, 211, 277,  89, 155, 224,
    290, 102, 168, 237, 303,  57, 123, 192, 258,  70,
    136, 205, 271,  83, 149, 218, 284,  96, 162, 231,
    297,  62, 128, 197, 263,  75, 141, 210, 276,  88,
    154, 223, 289, 101, 167, 236, 302,  58, 124, 193,
    259,  71, 137, 206, 272,  84, 150, 219, 285,  97,
    163, 232, 298,  59, 125, 194, 260,  64, 130, 199,
    265,  67, 133, 202, 268,  72, 138, 207, 273,  77,
    143, 212, 278,  80, 146, 215, 281,  85, 151, 220,
    286,  90, 156, 225, 291,  93, 159, 228, 294,  98,
    164, 233, 299, 103, 169, 238, 304, 106, 172, 241,
    307,  60, 126, 195, 261,  65, 131, 200, 266,  68,
    134, 203, 269,  73, 139, 208, 274,  78, 144, 213,
    279,  81, 147, 216, 282,  86, 152, 221, 287,  91,
    157, 226, 292,  94, 160, 229, 295,  99, 165, 234,
    300, 104, 170, 239, 305, 107, 173, 242, 308,  61,
    127, 196, 262,  66, 132, 201, 267,  69, 135, 204,
    270,  74, 140, 209, 275,  79, 145, 214, 280,  82,
    148, 217, 283,  87, 153, 222, 288,  92, 158, 227,
    293,  95, 161, 230, 296, 100, 166, 235, 301, 105,
    171, 240, 306, 108, 174, 243, 309
};

static pj_int16_t AMRWB_ordermap_1825[] =
{
      0,   4,   6, 121, 199, 280, 358,   7,   5,   3,
     47,  48,  49,  50,  51, 206, 207, 208, 209, 210,
    122, 200, 281, 359, 127, 205, 286, 364, 124, 202,
    283, 361, 125, 203, 284, 362, 128, 287, 126, 204,
    285, 363, 123, 201, 282, 360,  52,   2,   1, 129,
    288, 211,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    212,  31, 130, 289,   9,  33,  11, 131, 290,  54,
    213,  28,  27, 132, 291,  34,  35,  29,  46,  32,
     30,  55, 214,  37,  36,  39,  38,  40, 133, 292,
     41,  42,  43,  44,  45,  56, 134, 215, 293, 198,
    299, 136, 120, 138,  60, 279,  58,  62, 357, 139,
    140, 295, 156,  57, 219, 297,  63, 217, 137, 170,
    300, 222,  64, 106,  61,  78, 294,  92, 142, 141,
    135, 221, 296, 301, 343,  59, 298, 184, 329, 315,
    220, 216, 265, 251, 218, 237, 352, 223, 157,  86,
    171,  87, 164, 351, 111, 302,  65, 178, 115, 323,
     72, 192, 101, 179,  93,  73, 193, 151, 337, 309,
    143, 274,  69, 324, 165, 150,  97, 338, 110, 310,
    330, 273,  68, 107, 175, 245, 114,  79, 113, 189,
    246, 259, 174,  71, 185,  96, 344, 100, 322,  83,
    334, 316, 333, 252, 161, 348, 147,  82, 269, 232,
    260, 308, 353, 347, 163, 231, 306, 320, 188, 270,
    146, 177, 266, 350, 256,  85, 149, 116, 191, 160,
    238, 258, 336, 305, 255,  88, 224,  99, 339, 230,
    228, 227, 272, 242, 241, 319, 233, 311, 102,  74,
    180, 275,  66, 194, 152, 325, 172, 247, 244, 261,
    117, 158, 166, 354,  75, 144, 108, 312,  94, 186,
    303,  80, 234,  89, 195, 112, 340, 181, 345, 317,
    326, 276, 239, 167, 118, 313,  70, 355, 327, 253,
    190, 176, 271, 104,  98, 153, 103,  90,  76, 267,
    277, 248, 225, 262, 182,  84, 154, 235, 335, 168,
    331, 196, 341, 249, 162, 307, 148, 349, 263, 321,
    257, 243, 229, 356, 159, 119,  67, 187, 173, 145,
    240,  77, 304, 332, 314, 342, 109, 254,  81, 278,
    105,  91, 346, 318, 183, 250, 197, 328,  95, 155,
    169, 268, 226, 236, 264
};

static pj_int16_t AMRWB_ordermap_1985[] =
{
      0,   4,   6, 129, 215, 304, 390,   7,   5,   3,
     47,  48,  49,  50,  51, 222, 223, 224, 225, 226,
    130, 216, 305, 391, 135, 221, 310, 396, 132, 218,
    307, 393, 133, 219, 308, 394, 136, 311, 134, 220,
    309, 395, 131, 217, 306, 392,  52,   2,   1, 137,
    312, 227,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    228,  31, 138, 313,   9,  33,  11, 139, 314,  54,
    229,  28,  27, 140, 315,  34,  35,  29,  46,  32,
     30,  55, 230,  37,  36,  39,  38,  40, 141, 316,
     41,  42,  43,  44,  45,  56, 142, 231, 317,  63,
     73,  92, 340,  82, 324, 149, 353, 159, 334, 165,
    338, 178, 163, 254,  77, 168, 257, 153, 343,  57,
    248, 238,  79, 252, 166,  67,  80, 201, 101, 267,
    143, 164, 341, 255, 339, 187, 376, 318,  78, 328,
    362, 115, 232, 242, 253, 290, 276,  62,  58, 158,
     68,  93, 179, 319, 148, 169, 154,  72, 385, 329,
    333, 344, 102,  83, 144, 233, 323, 124, 243, 192,
    354, 237,  64, 247, 202, 209, 150, 116, 335, 268,
    239, 299, 188, 196, 298,  94, 195, 258, 123, 363,
    384, 109, 325, 371, 170, 370,  84, 110, 295, 180,
     74, 210, 191, 106, 291, 205, 367, 381, 377, 206,
    355, 122, 119, 120, 383, 160, 105, 108, 277, 380,
    294, 284, 285, 345, 208, 269, 249, 366, 386, 300,
    297, 259, 125, 369, 197,  97, 194, 286, 211, 281,
    280, 183, 372,  87, 155, 283,  59, 348, 327, 184,
     76, 111, 330, 203, 349,  69,  98, 152, 145, 189,
     66, 320, 337, 173, 358, 251, 198, 174, 263, 262,
    126, 241, 193,  88, 388, 117,  95, 387, 112, 359,
    287, 244, 103, 272, 301, 171, 162, 234, 273, 127,
    373, 181, 292,  85, 378, 302, 121, 107, 364, 346,
    356, 212, 278, 213,  65, 382, 288, 207, 113, 175,
     99, 296, 374, 368, 199, 260, 185, 336, 331, 161,
    270, 264, 250, 240,  75, 350, 151,  60,  89, 321,
    156, 274, 360, 326,  70, 282, 167, 146, 352,  81,
     91, 389, 266, 245, 177, 235, 190, 256, 204, 342,
    128, 118, 303, 104, 379, 182, 114, 375, 200,  96,
    293, 172, 214, 365, 279,  86, 289, 351, 347, 357,
    261, 186, 176, 271,  90, 100, 147, 322, 275, 361,
     71, 332,  61, 265, 157, 246, 236
};

static pj_int16_t AMRWB_ordermap_2305[] =
{
      0,   4,   6, 145, 247, 352, 454,   7,   5,   3,
     47,  48,  49,  50,  51, 254, 255, 256, 257, 258,
    146, 248, 353, 455, 151, 253, 358, 460, 148, 250,
    355, 457, 149, 251, 356, 458, 152, 359, 150, 252,
    357, 459, 147, 249, 354, 456,  52,   2,   1, 153,
    360, 259,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    260,  31, 154, 361,   9,  33,  11, 155, 362,  54,
    261,  28,  27, 156, 363,  34,  35,  29,  46,  32,
     30,  55, 262,  37,  36,  39,  38,  40, 157, 364,
     41,  42,  43,  44,  45,  56, 158, 263, 365, 181,
    192, 170,  79,  57, 399,  90, 159, 297, 377, 366,
    275,  68, 183, 388, 286, 194, 299,  92,  70, 182,
    401, 172,  59,  91,  58, 400, 368, 161,  81, 160,
    264, 171,  80, 389, 390, 378, 379, 193, 298,  69,
    266, 265, 367, 277, 288, 276, 287, 184,  60, 195,
     82,  93,  71, 369, 402, 173, 162, 444, 300, 391,
     98,  76, 278,  61, 267, 374, 135, 411, 167, 102,
    380, 200,  87, 178,  65,  94, 204, 124,  72, 342,
    189, 305, 381, 396, 433, 301, 226, 407, 289, 237,
    113, 215, 185, 128, 309, 403, 116, 320, 196, 331,
    370, 422, 174,  64, 392,  83, 425, 219, 134, 188,
    432, 112, 427, 139, 279, 163, 436, 208, 447, 218,
    236, 229,  97, 294, 385, 230, 166, 268, 177, 443,
    225, 426, 101, 272, 138, 127, 290, 117, 347, 199,
    414,  95, 140, 240, 410, 395, 209, 129, 283, 346,
    105, 241, 437,  86, 308, 448, 203, 345, 186, 107,
    220, 415, 334, 319, 106, 313, 118, 123,  73, 207,
    421, 214, 384, 373, 438,  62, 371, 341,  75, 449,
    168, 323, 164, 242, 416, 324, 304, 197, 335, 404,
    271,  63, 191, 325,  96, 169, 231, 280, 312, 187,
    406,  84, 201, 100,  67, 382, 175, 336, 202, 330,
    269, 393, 376, 383, 293, 307, 409, 179, 285, 314,
    302, 372, 398, 190, 180,  89,  99, 103, 232,  78,
     88,  77, 136, 387, 165, 198, 394, 125, 176, 428,
     74, 375, 238, 227,  66, 273, 282, 141, 306, 412,
    114,  85, 130, 348, 119, 291, 296, 386, 233, 397,
    303, 405, 284, 445, 423, 221, 210, 205, 450, 108,
    274, 434, 216, 343, 337, 142, 243, 321, 408, 451,
    310, 292, 120, 109, 281, 439, 270, 429, 332, 295,
    418, 211, 315, 222, 326, 131, 430, 244, 327, 349,
    417, 316, 143, 338, 440, 234, 110, 212, 452, 245,
    121, 419, 350, 223, 132, 441, 328, 413, 317, 339,
    126, 104, 137, 446, 344, 239, 435, 115, 333, 206,
    322, 217, 228, 424, 453, 311, 351, 111, 442, 224,
    213, 122, 431, 340, 235, 246, 133, 144, 420, 329,
    318
};

static pj_int16_t AMRWB_ordermap_2385[] =
{
      0,   4,   6, 145, 251, 360, 466,   7,   5,   3,
     47,  48,  49,  50,  51, 262, 263, 264, 265, 266,
    146, 252, 361, 467, 151, 257, 366, 472, 148, 254,
    363, 469, 149, 255, 364, 470, 156, 371, 150, 256,
    365, 471, 147, 253, 362, 468,  52,   2,   1, 157,
    372, 267,  19,  21,  12,  17,  18,  20,  16,  25,
     13,  10,  14,  24,  23,  22,  26,   8,  15,  53,
    268,  31, 152, 153, 154, 155, 258, 259, 260, 261,
    367, 368, 369, 370, 473, 474, 475, 476, 158, 373,
      9,  33,  11, 159, 374,  54, 269,  28,  27, 160,
    375,  34,  35,  29,  46,  32,  30,  55, 270,  37,
     36,  39,  38,  40, 161, 376,  41,  42,  43,  44,
     45,  56, 162, 271, 377, 185, 196, 174,  79,  57,
    411,  90, 163, 305, 389, 378, 283,  68, 187, 400,
    294, 198, 307,  92,  70, 186, 413, 176,  59,  91,
     58, 412, 380, 165,  81, 164, 272, 175,  80, 401,
    402, 390, 391, 197, 306,  69, 274, 273, 379, 285,
    296, 284, 295, 188,  60, 199,  82,  93,  71, 381,
    414, 177, 166, 456, 308, 403,  98,  76, 286,  61,
    275, 386, 135, 423, 171, 102, 392, 204,  87, 182,
     65,  94, 208, 124,  72, 350, 193, 313, 393, 408,
    445, 309, 230, 419, 297, 241, 113, 219, 189, 128,
    317, 415, 116, 328, 200, 339, 382, 434, 178,  64,
    404,  83, 437, 223, 134, 192, 444, 112, 439, 139,
    287, 167, 448, 212, 459, 222, 240, 233,  97, 302,
    397, 234, 170, 276, 181, 455, 229, 438, 101, 280,
    138, 127, 298, 117, 355, 203, 426,  95, 140, 244,
    422, 407, 213, 129, 291, 354, 105, 245, 449,  86,
    316, 460, 207, 353, 190, 107, 224, 427, 342, 327,
    106, 321, 118, 123,  73, 211, 433, 218, 396, 385,
    450,  62, 383, 349,  75, 461, 172, 331, 168, 246,
    428, 332, 312, 201, 343, 416, 279,  63, 195, 333,
     96, 173, 235, 288, 320, 191, 418,  84, 205, 100,
     67, 394, 179, 344, 206, 338, 277, 405, 388, 395,
    301, 315, 421, 183, 293, 322, 310, 384, 410, 194,
    184,  89,  99, 103, 236,  78,  88,  77, 136, 399,
    169, 202, 406, 125, 180, 440,  74, 387, 242, 231,
     66, 281, 290, 141, 314, 424, 114,  85, 130, 356,
    119, 299, 304, 398, 237, 409, 311, 417, 292, 457,
    435, 225, 214, 209, 462, 108, 282, 446, 220, 351,
    345, 142, 247, 329, 420, 463, 318, 300, 120, 109,
    289, 451, 278, 441, 340, 303, 430, 215, 323, 226,
    334, 131, 442, 248, 335, 357, 429, 324, 143, 346,
    452, 238, 110, 216, 464, 249, 121, 431, 358, 227,
    132, 453, 336, 425, 325, 347, 126, 104, 137, 458,
    352, 243, 447, 115, 341, 210, 330, 221, 232, 436,
    465, 319, 359, 111, 454, 228, 217, 122, 443, 348,
    239, 250, 133, 144, 432, 337, 326
};

static pj_int16_t *AMRWB_ordermaps[9] =
{
    AMRWB_ordermap_660,
    AMRWB_ordermap_885,
    AMRWB_ordermap_1265,
    AMRWB_ordermap_1425,
    AMRWB_ordermap_1585,
    AMRWB_ordermap_1825,
    AMRWB_ordermap_1985,
    AMRWB_ordermap_2305,
    AMRWB_ordermap_2385
};

static pj_uint8_t AMRNB_framelen[16] = 
    {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 5};
static pj_uint16_t AMRNB_framelenbits[9] = 
    {95, 103, 118, 134, 148, 159, 204, 244, 39};
static pj_uint16_t AMRNB_bitrates[8] = 
    {4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200};

static pj_uint8_t AMRWB_framelen[16] = 
    {17, 23, 32, 37, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 0, 5};
static pj_uint16_t AMRWB_framelenbits[10] = 
    {132, 177, 253, 285, 317, 365, 397, 461, 477, 40};
static pj_uint16_t AMRWB_bitrates[9] = 
    {6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850};


/* Get mode based on bitrate */
static int amr_get_mode(unsigned bitrate)
{
    int mode = -1;

    if(bitrate==4750){
	mode = 0;
    } else if(bitrate==5150){
	mode = 1;
    } else if(bitrate==5900){
	mode = 2;
    } else if(bitrate==6700){
	mode = 3;
    } else if(bitrate==7400){
	mode = 4;
    } else if(bitrate==7950){
	mode = 5;
    } else if(bitrate==10200){
	mode = 6;
    } else if(bitrate==12200){
	mode = 7;

    /* AMRWB */
    } else if(bitrate==6600){
	mode = 0;
    } else if(bitrate==8850){
	mode = 1;
    } else if(bitrate==12650){
	mode = 2;
    } else if(bitrate==14250){
	mode = 3;
    } else if(bitrate==15850){
	mode = 4;
    } else if(bitrate==18250){
	mode = 5;
    } else if(bitrate==19850){
	mode = 6;
    } else if(bitrate==23050){
	mode = 7;
    } else if(bitrate==23850){
	mode = 8;
    }
    return mode;
}

/* Rearrange AMR bitstream of rtp_frame:
 * - make the start_bit to be 0
 * - if it is speech frame, reorder bitstream from sensitivity bits order
 *   to encoder bits order.
 */
static void predecode_amr( ipp_private_t *codec_data,
			   const pjmedia_frame *rtp_frame,
			   USC_Bitstream *usc_frame)
{
    pj_uint8_t FT, Q;
    pj_int8_t amr_bits[477 + 7] = {0};
    pj_int8_t *p_amr_bits = &amr_bits[0];
    unsigned i;
    /* read cursor */
    pj_uint8_t *r = (pj_uint8_t*)rtp_frame->buf;
    pj_uint8_t start_bit;
    /* write cursor */
    pj_uint8_t *w = (pj_uint8_t*)rtp_frame->buf;
    /* env vars for AMR or AMRWB */
    pj_bool_t AMRWB;
    pj_uint8_t SID_FT = 8;
    pj_uint8_t *framelen_tbl = AMRNB_framelen;
    pj_uint16_t *framelenbit_tbl = AMRNB_framelenbits;
    pj_uint16_t *bitrate_tbl = AMRNB_bitrates;
    pj_int16_t **order_map = AMRNB_ordermaps;

    AMRWB = (ipp_codec[codec_data->codec_idx].pt == PJMEDIA_RTP_PT_AMRWB);
    if (AMRWB) {
	SID_FT = 9;
	framelen_tbl = AMRWB_framelen;
	framelenbit_tbl = AMRWB_framelenbits;
	bitrate_tbl = AMRWB_bitrates;
	order_map = AMRWB_ordermaps;
    }

    start_bit = (pj_uint8_t)((rtp_frame->bit_info & 0x0700) >> 8);
    FT = (pj_uint8_t)(rtp_frame->bit_info & 0x0F);
    Q = (pj_uint8_t)((rtp_frame->bit_info >> 16) & 0x01);

    /* unpack AMR bitstream if there is any data */
    if (FT <= SID_FT) {
	i = 0;
	if (start_bit) {
	    for (; i < (unsigned)(8-start_bit); ++i)
		*p_amr_bits++ = (pj_uint8_t)((*r >> (7-start_bit-i)) & 1);
	    ++r;
	}
	for(; i < framelenbit_tbl[FT]; i += 8) {
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 7) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 6) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 5) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 4) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 3) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 2) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 1) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r ) & 1);
	    ++r;
	}
    }

    if (FT < SID_FT) {
	/* Speech */
	pj_int16_t *order_map_;

	order_map_ = order_map[FT];
	pj_bzero(rtp_frame->buf, rtp_frame->size);
	for(i = 0; i < framelenbit_tbl[FT]; ++i) {
	    if (amr_bits[i]) {
		pj_uint16_t bitpos;
		bitpos = order_map_[i];
		w[bitpos>>3] |= 1 << (7 - (bitpos % 8));
	    }
	}
	usc_frame->nbytes = framelen_tbl[FT];
	if (Q)
	    usc_frame->frametype = 0;
	else
	    usc_frame->frametype = AMRWB ? 6 : 5;
	usc_frame->bitrate = bitrate_tbl[FT];
    } else if (FT == SID_FT) {
	/* SID */
	pj_uint8_t w_bitptr = 0;
	pj_uint8_t STI;
	pj_uint8_t FT_;

	STI = amr_bits[35];
	if (AMRWB)
	    FT_ = (pj_uint8_t)((amr_bits[36] << 3) | (amr_bits[37] << 2) |
		               (amr_bits[38] << 1) | amr_bits[39]);
	else
	    FT_ = (pj_uint8_t)((amr_bits[36] << 2) | (amr_bits[37] << 1) | 
	                       amr_bits[38]);

	pj_bzero(rtp_frame->buf, rtp_frame->size);
	for(i = 0; i < framelenbit_tbl[FT]; ++i) {
	    if (amr_bits[i])
		*w |= (1 << (7-w_bitptr));

	    if (++w_bitptr == 8) {
		++w;
		w_bitptr = 0;
	    }
	}

	usc_frame->nbytes = 5;
	if (Q)
	    usc_frame->frametype = STI? 2 : 1;
	else
	    usc_frame->frametype = AMRWB ? 7 : 6;
	
	usc_frame->bitrate = bitrate_tbl[FT_];
    } else {
	/* NO DATA */
	usc_frame->nbytes = 0;
	usc_frame->frametype = 3;
	usc_frame->bitrate = 0;
    }

    usc_frame->pBuffer = rtp_frame->buf;
}

static pj_status_t pack_amr(ipp_private_t *codec_data, void *pkt, 
			    pj_size_t *pkt_size, pj_size_t max_pkt_size)
{
    /* Settings */
    pj_uint8_t CMR = 15; /* We don't request any code mode */
    pj_uint8_t octet_aligned = 0; /* default==0 when SDP not specifying */
    /* Write cursor */
    pj_uint8_t *w = (pj_uint8_t*)pkt;
    pj_uint8_t w_bitptr = 0;
    /* Read cursor */
    pj_uint8_t *r;
    /* env vars for AMR or AMRWB */
    pj_bool_t AMRWB;
    pj_uint8_t SID_FT = 8;
    pj_uint8_t *framelen_tbl = AMRNB_framelen;
    pj_uint16_t *framelenbit_tbl = AMRNB_framelenbits;
    pj_uint16_t *bitrate_tbl = AMRNB_bitrates;
    pj_int16_t **order_map = AMRNB_ordermaps;

    AMRWB = ipp_codec[codec_data->codec_idx].pt == PJMEDIA_RTP_PT_AMRWB;
    if (AMRWB) {
	SID_FT = 9;
	framelen_tbl = AMRWB_framelen;
	framelenbit_tbl = AMRWB_framelenbits;
	bitrate_tbl = AMRWB_bitrates;
	order_map = AMRWB_ordermaps;
    }

    PJ_TODO(Make_sure_buffer_is_enough_for_packing_AMR_packet);

    r = (pj_uint8_t*)pkt + max_pkt_size - *pkt_size;
    
    /* Align pkt buf right */
    pj_memmove(r, w, *pkt_size);

    /* Code Mode Request, 4 bits */
    *w = (pj_uint8_t)(CMR << 4);
    w_bitptr = 4;
    if (octet_aligned) {
	++w;
	w_bitptr = 0;
    }

    /* Table Of Contents, 6 bits each */
    for (;;) {
	pj_uint8_t TOC;
	pj_uint8_t F, FT, Q;

	F = (pj_uint8_t)((*r & 0x40) == 0);
	FT = (pj_uint8_t)(*r & 0x0F);
	Q = (pj_uint8_t)((*r & 0x80) == 0);

	pj_assert(FT <= SID_FT || FT == 14 || FT == 15);
	TOC = (pj_uint8_t)((F<<5) | (FT<<1) | Q);

	if (w_bitptr == 0) {
	    *w = (pj_uint8_t)(TOC<<2);
	    w_bitptr = 6;
	} else if (w_bitptr == 2) {
	    *w++ |= TOC;
	    w_bitptr = 0;
	} else if (w_bitptr == 4) {
	    *w++ |= TOC>>2;
	    *w = (pj_uint8_t)(TOC<<6);
	    w_bitptr = 2;
	} else if (w_bitptr == 6) {
	    *w++ |= TOC>>4;
	    *w = (pj_uint8_t)(TOC<<4);
	    w_bitptr = 4;
	}

	if (octet_aligned) {
	    ++w;
	    w_bitptr = 0;
	}

	if (FT > SID_FT)
	    /* NO DATA */
	    r += 1;
	else
	    r += framelen_tbl[FT] + 1;

	/* Last frame */
	if (!F)
	    break;
    }

    /* Speech frames */
    r = (pj_uint8_t*)pkt + max_pkt_size - *pkt_size;

    for (;;) {
	pj_uint8_t F, FT;
	pj_int8_t amr_bits[477 + 7] = {0};
	pj_int8_t *p_amr_bits = &amr_bits[0];
	unsigned i;

	F = (pj_uint8_t)((*r & 0x40) == 0);
	FT = (pj_uint8_t)(*r & 0x0F);
	pj_assert(FT <= SID_FT || FT == 14 || FT == 15);

	++r;
	if (FT > SID_FT) {
	    if (!F)
		break;
	    continue;
	}

	/* Unpack bits */
	for(i = 0; i < framelen_tbl[FT]; ++i) {
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 7) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 6) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 5) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 4) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 3) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 2) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r >> 1) & 1);
	    *p_amr_bits++ = (pj_uint8_t)((*r ) & 1);
	    ++r;
	}

	if (FT < SID_FT) {
	    /* Speech */
	    pj_int16_t *order_map_;

	    /* Put bits in the packet, sensitivity descending ordered */
	    order_map_ = order_map[FT];
	    if (w_bitptr == 0) *w = 0;
	    for(i = 0; i < framelenbit_tbl[FT]; ++i) {
		pj_uint8_t bit;
		bit = amr_bits[order_map_[i]];
		
		if (bit)
		    *w |= (1 << (7-w_bitptr));

		if (++w_bitptr == 8) {
		    w_bitptr = 0;
		    ++w;
		    *w = 0;
		}
	    }

	    if (octet_aligned) {
		++w;
		w_bitptr = 0;
	    }
	} else if (FT == SID_FT) {
	    /* SID */
	    pj_uint8_t STI = 0;

	    amr_bits[35] = (pj_uint8_t)(STI & 1);

	    if (AMRWB) {
		amr_bits[36] = (pj_uint8_t)((FT >> 3) & 1);
		amr_bits[37] = (pj_uint8_t)((FT >> 2) & 1);
		amr_bits[38] = (pj_uint8_t)((FT >> 1) & 1);
		amr_bits[39] = (pj_uint8_t)((FT) & 1);
	    } else {
		amr_bits[36] = (pj_uint8_t)((FT >> 2) & 1);
		amr_bits[37] = (pj_uint8_t)((FT >> 1) & 1);
		amr_bits[38] = (pj_uint8_t)((FT) & 1);
	    }

	    if (w_bitptr == 0) *w = 0;
	    for(i = 0; i < framelenbit_tbl[FT]; ++i) {
		if (amr_bits[i])
		    *w |= (1 << (7-w_bitptr));

		if (++w_bitptr == 8) {
		    w_bitptr = 0;
		    ++w;
		    *w = 0;
		}
	    }

	    if (octet_aligned) {
		++w;
		w_bitptr = 0;
	    }
	}

	if (!F)
	    break;
    }

    *pkt_size = w - (pj_uint8_t*)pkt;
    if (w_bitptr)
	*pkt_size += 1;

    pj_assert(*pkt_size <= max_pkt_size);

    return PJ_SUCCESS;
}


/* Parse AMR payload into frames. Frame.bit_info will contain start_bit and
 * AMR frame type, it is mapped as below (bit 0:MSB - bit 31:LSB)
 * - bit  0-16: degraded quality flag (Q)
 * - bit 17-24: start_bit
 * - bit 25-32: frame_type (FT)
 */
static pj_status_t parse_amr(ipp_private_t *codec_data, void *pkt, 
			     pj_size_t pkt_size, const pj_timestamp *ts,
			     unsigned *frame_cnt, pjmedia_frame frames[])
{
    unsigned cnt = 0;
    pj_timestamp ts_ = *ts;
    /* Settings */
    pj_uint8_t CMR = 15; /* See if remote request code mode */
    pj_uint8_t octet_aligned = 0; /* default==0 when SDP not specifying */
    /* Read cursor */
    pj_uint8_t r_bitptr = 0;
    pj_uint8_t *r = (pj_uint8_t*)pkt;
    /* env vars for AMR or AMRWB */
    pj_bool_t AMRWB;
    pj_uint8_t SID_FT = 8;
    pj_uint8_t *framelen_tbl = AMRNB_framelen;
    pj_uint16_t *framelenbit_tbl = AMRNB_framelenbits;

    PJ_UNUSED_ARG(pkt_size);

    AMRWB = ipp_codec[codec_data->codec_idx].pt == PJMEDIA_RTP_PT_AMRWB;
    if (AMRWB) {
	SID_FT = 9;
	framelen_tbl = AMRWB_framelen;
	framelenbit_tbl = AMRWB_framelenbits;
    }

    *frame_cnt = 0;

    /* Code Mode Request, 4 bits */
    CMR = (pj_uint8_t)((*r >> 4) & 0x0F);
    r_bitptr = 4;
    if (octet_aligned) {
	++r;
	r_bitptr = 0;
    }

    /* Table Of Contents, 6 bits each */
    for (;;) {
	pj_uint8_t TOC = 0;
	pj_uint8_t F, FT, Q;

	if (r_bitptr == 0) {
	    TOC = (pj_uint8_t)(*r >> 2);
	    r_bitptr = 6;
	} else if (r_bitptr == 2) {
	    TOC = (pj_uint8_t)(*r++ & 0x3F);
	    r_bitptr = 0;
	} else if (r_bitptr == 4) {
	    TOC = (pj_uint8_t)((*r++ & 0x0f) << 2);
	    TOC |= *r >> 6;
	    r_bitptr = 2;
	} else if (r_bitptr == 6) {
	    TOC = (pj_uint8_t)((*r++ & 0x03) << 4);
	    TOC |= *r >> 4;
	    r_bitptr = 4;
	}

	F = (pj_uint8_t)(TOC >> 5);
	FT = (pj_uint8_t)((TOC >> 1) & 0x0F);
	Q = (pj_uint8_t)(TOC & 1);

	if (FT > SID_FT && FT < 14) {
	    pj_assert(!"Invalid AMR frametype, stream may be corrupted!");
	    break;
	}

	if (octet_aligned) {
	    ++r;
	    r_bitptr = 0;
	}

	/* Set frame attributes */
	frames[cnt].bit_info = FT | (Q << 16);
	frames[cnt].timestamp = ts_;
	frames[cnt].type = PJMEDIA_FRAME_TYPE_AUDIO;

	ts_.u64 += ipp_codec[codec_data->codec_idx].samples_per_frame;
	++cnt;

	if (!F || cnt == *frame_cnt)
	    break;
    }
    *frame_cnt = cnt;

    cnt = 0;

    /* Speech frames */
    while (cnt < *frame_cnt) {
	unsigned FT;

	FT = frames[cnt].bit_info & 0x0F;

	frames[cnt].bit_info |= (r_bitptr << 8);
	frames[cnt].buf = r;

	if (octet_aligned) {
	    r += framelen_tbl[FT];
	    frames[cnt].size = framelen_tbl[FT];
	} else {
	    if (FT == 14 || FT == 15) {
		/* NO DATA */
		frames[cnt].size = 0;
	    } else {
		unsigned adv_bit;

		adv_bit = framelenbit_tbl[FT] + r_bitptr;
		r += adv_bit >> 3;
		r_bitptr = (pj_uint8_t)(adv_bit % 8);

		frames[cnt].size = adv_bit >> 3;
		if (r_bitptr)
		    ++frames[cnt].size;
	    }
	}
	++cnt;
    }

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_INTEL_IPP_CODEC_AMR */


#if defined(_MSC_VER) && PJMEDIA_AUTO_LINK_IPP_LIBS
#   pragma comment( lib, "ippcore.lib")
#   pragma comment( lib, "ipps.lib")
#   pragma comment( lib, "ippsc.lib")
#   pragma comment( lib, "ippsr.lib")
#   pragma comment( lib, "usc.lib")
#endif


#endif	/* PJMEDIA_HAS_INTEL_IPP */


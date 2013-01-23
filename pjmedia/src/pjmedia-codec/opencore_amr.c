/* $Id$ */
/* 
 * Copyright (C) 2011-2013 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2011 Dan Arrhenius <dan@keystream.se>
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

/* 
 * AMR codec implementation with OpenCORE AMR library
 */
#include <pjmedia-codec/g722.h>
#include <pjmedia-codec/amr_sdp_match.h>
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
#include <pj/math.h>

#if defined(PJMEDIA_HAS_OPENCORE_AMRNB_CODEC) && \
    (PJMEDIA_HAS_OPENCORE_AMRNB_CODEC != 0)
#define USE_AMRNB
#endif

#if defined(PJMEDIA_HAS_OPENCORE_AMRWB_CODEC) && \
    (PJMEDIA_HAS_OPENCORE_AMRWB_CODEC != 0)
#define USE_AMRWB
#endif

#if defined(USE_AMRNB) || defined(USE_AMRWB)

#ifdef USE_AMRNB
#include <opencore-amrnb/interf_enc.h>
#include <opencore-amrnb/interf_dec.h>
#endif

#ifdef USE_AMRWB
#include <vo-amrwbenc/enc_if.h>
#include <opencore-amrwb/dec_if.h>
#endif

#include <pjmedia-codec/amr_helper.h>
#include <pjmedia-codec/opencore_amr.h>

#define THIS_FILE "opencore_amr.c"

/* Tracing */
#define PJ_TRACE    0

#if PJ_TRACE
#   define TRACE_(expr)	PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif

/* Use PJMEDIA PLC */
#define USE_PJMEDIA_PLC	    1

#define FRAME_LENGTH_MS     20


/* Prototypes for AMR factory */
static pj_status_t amr_test_alloc(pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t amr_default_attr(pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t amr_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t amr_alloc_codec(pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t amr_dealloc_codec(pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for AMR implementation. */
static pj_status_t  amr_codec_init(pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  amr_codec_open(pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  amr_codec_close(pjmedia_codec *codec );
static pj_status_t  amr_codec_modify(pjmedia_codec *codec, 
				      const pjmedia_codec_param *attr );
static pj_status_t  amr_codec_parse(pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[]);
static pj_status_t  amr_codec_encode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  amr_codec_decode(pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  amr_codec_recover(pjmedia_codec *codec,
				      unsigned output_buf_len,
				      struct pjmedia_frame *output);



/* Definition for AMR codec operations. */
static pjmedia_codec_op amr_op = 
{
    &amr_codec_init,
    &amr_codec_open,
    &amr_codec_close,
    &amr_codec_modify,
    &amr_codec_parse,
    &amr_codec_encode,
    &amr_codec_decode,
    &amr_codec_recover
};

/* Definition for AMR codec factory operations. */
static pjmedia_codec_factory_op amr_factory_op =
{
    &amr_test_alloc,
    &amr_default_attr,
    &amr_enum_codecs,
    &amr_alloc_codec,
    &amr_dealloc_codec,
    &pjmedia_codec_opencore_amrnb_deinit
};


/* AMR factory */
static struct amr_codec_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_bool_t                init[2];
} amr_codec_factory;


/* AMR codec private data. */
struct amr_data
{
    pj_pool_t		*pool;
    unsigned             clock_rate;
    void		*encoder;
    void		*decoder;
    pj_bool_t		 plc_enabled;
    pj_bool_t		 vad_enabled;
    int			 enc_mode;
    pjmedia_codec_amr_pack_setting enc_setting;
    pjmedia_codec_amr_pack_setting dec_setting;
#if USE_PJMEDIA_PLC
    pjmedia_plc		*plc;
#endif
    pj_timestamp	 last_tx;
};

/* Index for AMR tables. */
enum
{
    IDX_AMR_NB,	/* Index for narrowband.    */
    IDX_AMR_WB	/* Index for wideband.      */
};

static pjmedia_codec_amr_config def_config[2] =
{{ /* AMR-NB */
    PJ_FALSE,	    /* octet align	*/
    5900	    /* bitrate		*/
 },
 { /* AMR-WB */
    PJ_FALSE,	    /* octet align	*/
    12650	    /* bitrate		*/
 }};

static const pj_uint16_t* amr_bitrates[2] =
    {pjmedia_codec_amrnb_bitrates, pjmedia_codec_amrwb_bitrates};


/*
 * Initialize and register AMR codec factory to pjmedia endpoint.
 */
static pj_status_t amr_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_str_t codec_name;
    pj_status_t status;

    if (amr_codec_factory.pool != NULL)
	return PJ_SUCCESS;

    /* Create AMR codec factory. */
    amr_codec_factory.base.op = &amr_factory_op;
    amr_codec_factory.base.factory_data = NULL;
    amr_codec_factory.endpt = endpt;

    amr_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "amr", 1000, 
						       1000);
    if (!amr_codec_factory.pool)
	return PJ_ENOMEM;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register format match callback. */
    pj_cstr(&codec_name, "AMR");
    status = pjmedia_sdp_neg_register_fmt_match_cb(
					&codec_name,
					&pjmedia_codec_amr_match_sdp);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&amr_codec_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(amr_codec_factory.pool);
    amr_codec_factory.pool = NULL;
    return status;
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_init( pjmedia_endpt *endpt )
{
    amr_codec_factory.init[IDX_AMR_NB] = PJ_TRUE;
    
    return amr_init(endpt);
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrwb_init( pjmedia_endpt *endpt )
{
    amr_codec_factory.init[IDX_AMR_WB] = PJ_TRUE;
    
    return amr_init(endpt);    
}


/*
 * Unregister AMR codec factory from pjmedia endpoint and deinitialize
 * the AMR codec library.
 */
static pj_status_t amr_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (amr_codec_factory.init[IDX_AMR_NB] ||
        amr_codec_factory.init[IDX_AMR_WB])
    {
        return PJ_SUCCESS;
    }
    
    if (amr_codec_factory.pool == NULL)
	return PJ_SUCCESS;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(amr_codec_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(amr_codec_factory.pool);
	amr_codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister AMR codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &amr_codec_factory.base);
    
    /* Destroy pool. */
    pj_pool_release(amr_codec_factory.pool);
    amr_codec_factory.pool = NULL;
    
    return status;
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_deinit(void)
{
    amr_codec_factory.init[IDX_AMR_NB] = PJ_FALSE;
    
    return amr_deinit();
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrwb_deinit(void)
{
    amr_codec_factory.init[IDX_AMR_WB] = PJ_FALSE;
    
    return amr_deinit();
}

static pj_status_t
amr_set_config(unsigned idx, const pjmedia_codec_amr_config *config)
{
    unsigned nbitrates;

    def_config[idx] = *config;

    /* Normalize bitrate. */
    nbitrates = PJ_ARRAY_SIZE(amr_bitrates[idx]);
    if (def_config[idx].bitrate < amr_bitrates[idx][0]) {
	def_config[idx].bitrate = amr_bitrates[idx][0];
    } else if (def_config[idx].bitrate > amr_bitrates[idx][nbitrates-1]) {
	def_config[idx].bitrate = amr_bitrates[idx][nbitrates-1];
    } else
    {
	unsigned i;
	
	for (i = 0; i < nbitrates; ++i) {
	    if (def_config[idx].bitrate <= amr_bitrates[idx][i])
		break;
	}
	def_config[idx].bitrate = amr_bitrates[idx][i];
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrnb_set_config(
                                    const pjmedia_codec_amrnb_config *config)
{
    return amr_set_config(IDX_AMR_NB, (const pjmedia_codec_amr_config *)config);
}

PJ_DEF(pj_status_t) pjmedia_codec_opencore_amrwb_set_config(
                                    const pjmedia_codec_amrwb_config *config)
{
    return amr_set_config(IDX_AMR_WB, (const pjmedia_codec_amr_config *)config);
}

/*
 * Check if factory can allocate the specified codec.
 */
static pj_status_t amr_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    const pj_str_t amr_tag = { "AMR", 3};
    const pj_str_t amrwb_tag = { "AMR-WB", 6};
    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;
    
    /* Check payload type. */
    if (info->pt != PJMEDIA_RTP_PT_AMR && info->pt != PJMEDIA_RTP_PT_AMRWB)
	return PJMEDIA_CODEC_EUNSUP;
    
    /* Check encoding name. */
    if (pj_stricmp(&info->encoding_name, &amr_tag) != 0 &&
        pj_stricmp(&info->encoding_name, &amrwb_tag) != 0)
    {
	return PJMEDIA_CODEC_EUNSUP;
    }
    
    /* Check clock-rate */
    if ((info->clock_rate == 8000 && amr_codec_factory.init[IDX_AMR_NB]) ||
        (info->clock_rate == 16000 && amr_codec_factory.init[IDX_AMR_WB]))
    {
        return PJ_SUCCESS;
    }

    /* Unsupported or disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Generate default attribute.
 */
static pj_status_t amr_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr )
{
    unsigned idx;
    
    PJ_UNUSED_ARG(factory);

    idx = (id->clock_rate <= 8000? IDX_AMR_NB: IDX_AMR_WB);
    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.clock_rate = (id->clock_rate <= 8000? 8000: 16000);
    attr->info.channel_cnt = 1;
    attr->info.avg_bps = def_config[idx].bitrate;
    attr->info.max_bps = amr_bitrates[idx][PJ_ARRAY_SIZE(amr_bitrates[idx])-1];
    attr->info.pcm_bits_per_sample = 16;
    attr->info.frm_ptime = 20;
    attr->info.pt = (pj_uint8_t)id->pt;

    attr->setting.frm_per_pkt = 2;
    attr->setting.vad = 1;
    attr->setting.plc = 1;

    if (def_config[idx].octet_align) {
	attr->setting.dec_fmtp.cnt = 1;
	attr->setting.dec_fmtp.param[0].name = pj_str("octet-align");
	attr->setting.dec_fmtp.param[0].val = pj_str("1");
    }

    /* Default all other flag bits disabled. */

    return PJ_SUCCESS;
}


/*
 * Enum codecs supported by this factory (i.e. AMR-NB and AMR-WB).
 */
static pj_status_t amr_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    *count = 0;

    if (amr_codec_factory.init[IDX_AMR_NB]) {
        pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
        codecs[*count].encoding_name = pj_str("AMR");
        codecs[*count].pt = PJMEDIA_RTP_PT_AMR;
        codecs[*count].type = PJMEDIA_TYPE_AUDIO;
        codecs[*count].clock_rate = 8000;
        codecs[*count].channel_cnt = 1;
        (*count)++;
    }
    
    if (amr_codec_factory.init[IDX_AMR_NB]) {
        pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
        codecs[*count].encoding_name = pj_str("AMR-WB");
        codecs[*count].pt = PJMEDIA_RTP_PT_AMRWB;
        codecs[*count].type = PJMEDIA_TYPE_AUDIO;
        codecs[*count].clock_rate = 16000;
        codecs[*count].channel_cnt = 1;
        (*count)++;
    }

    return PJ_SUCCESS;
}


/*
 * Allocate a new AMR codec instance.
 */
static pj_status_t amr_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_codec *codec;
    struct amr_data *amr_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &amr_codec_factory.base, PJ_EINVAL);

    pool = pjmedia_endpt_create_pool(amr_codec_factory.endpt, "amr-inst", 
				     512, 512);

    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
    codec->op = &amr_op;
    codec->factory = factory;

    amr_data = PJ_POOL_ZALLOC_T(pool, struct amr_data);
    codec->codec_data = amr_data;
    amr_data->pool = pool;

#if USE_PJMEDIA_PLC
    /* Create PLC */
    status = pjmedia_plc_create(pool, id->clock_rate,
                                id->clock_rate * FRAME_LENGTH_MS / 1000, 0,
                                &amr_data->plc);
    if (status != PJ_SUCCESS) {
	return status;
    }
#else
    PJ_UNUSED_ARG(status);
#endif
    *p_codec = codec;
    return PJ_SUCCESS;
}


/*
 * Free codec.
 */
static pj_status_t amr_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct amr_data *amr_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &amr_codec_factory.base, PJ_EINVAL);

    amr_data = (struct amr_data*) codec->codec_data;

    /* Close codec, if it's not closed. */
    amr_codec_close(codec);

    pj_pool_release(amr_data->pool);
    amr_data = NULL;

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t amr_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}


/*
 * Open codec.
 */
static pj_status_t amr_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pjmedia_codec_amr_pack_setting *setting;
    unsigned i;
    pj_uint8_t octet_align = 0;
    pj_int8_t enc_mode;
    const pj_str_t STR_FMTP_OCTET_ALIGN = {"octet-align", 11};
    unsigned idx;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(amr_data != NULL, PJ_EINVALIDOP);

    idx = (attr->info.clock_rate <= 8000? IDX_AMR_NB: IDX_AMR_WB);
    enc_mode = pjmedia_codec_amr_get_mode(attr->info.avg_bps);
    pj_assert(enc_mode >= 0 &&
              enc_mode < PJ_ARRAY_SIZE(amr_bitrates[idx]));

    /* Check octet-align */
    for (i = 0; i < attr->setting.dec_fmtp.cnt; ++i) {
	if (pj_stricmp(&attr->setting.dec_fmtp.param[i].name, 
		       &STR_FMTP_OCTET_ALIGN) == 0)
	{
	    octet_align = (pj_uint8_t)
			  (pj_strtoul(&attr->setting.dec_fmtp.param[i].val));
	    break;
	}
    }

    /* Check mode-set */
    for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	const pj_str_t STR_FMTP_MODE_SET = {"mode-set", 8};
        
	if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name, 
		       &STR_FMTP_MODE_SET) == 0)
	{
	    const char *p;
	    pj_size_t l;
	    pj_int8_t diff = 99;

	    /* Encoding mode is chosen based on local default mode setting:
	     * - if local default mode is included in the mode-set, use it
	     * - otherwise, find the closest mode to local default mode;
	     *   if there are two closest modes, prefer to use the higher
	     *   one, e.g: local default mode is 4, the mode-set param
	     *   contains '2,3,5,6', then 5 will be chosen.
	     */
	    p = pj_strbuf(&attr->setting.enc_fmtp.param[i].val);
	    l = pj_strlen(&attr->setting.enc_fmtp.param[i].val);
	    while (l--) {
		if (*p>='0' &&
                    *p<=('0'+PJ_ARRAY_SIZE(amr_bitrates[idx])-1))
                {
		    pj_int8_t tmp = *p - '0' - enc_mode;

		    if (PJ_ABS(diff) > PJ_ABS(tmp) || 
			(PJ_ABS(diff) == PJ_ABS(tmp) && tmp > diff))
		    {
			diff = tmp;
			if (diff == 0) break;
		    }
		}
		++p;
	    }
	    PJ_ASSERT_RETURN(diff != 99, PJMEDIA_CODEC_EFAILED);

	    enc_mode = enc_mode + diff;

	    break;
	}
    }

    amr_data->clock_rate = attr->info.clock_rate;
    amr_data->vad_enabled = (attr->setting.vad != 0);
    amr_data->plc_enabled = (attr->setting.plc != 0);
    amr_data->enc_mode = enc_mode;

    if (idx == IDX_AMR_NB) {
#ifdef USE_AMRNB
        amr_data->encoder = Encoder_Interface_init(amr_data->vad_enabled);
#endif
    } else {
#ifdef USE_AMRWB
        amr_data->encoder = E_IF_init();
#endif
    }
    if (amr_data->encoder == NULL) {
	TRACE_((THIS_FILE, "Encoder initialization failed"));
	amr_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    setting = &amr_data->enc_setting;
    pj_bzero(setting, sizeof(pjmedia_codec_amr_pack_setting));
    setting->amr_nb = (idx == IDX_AMR_NB? 1: 0);
    setting->reorder = 0;
    setting->octet_aligned = octet_align;
    setting->cmr = 15;

    if (idx == IDX_AMR_NB) {
#ifdef USE_AMRNB
        amr_data->decoder = Decoder_Interface_init();
#endif
    } else {
#ifdef USE_AMRWB
        amr_data->decoder = D_IF_init();
#endif
    }
    if (amr_data->decoder == NULL) {
	TRACE_((THIS_FILE, "Decoder initialization failed"));
	amr_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    setting = &amr_data->dec_setting;
    pj_bzero(setting, sizeof(pjmedia_codec_amr_pack_setting));
    setting->amr_nb = (idx == IDX_AMR_NB? 1: 0);
    setting->reorder = 0;
    setting->octet_aligned = octet_align;

    TRACE_((THIS_FILE, "AMR codec allocated: clockrate=%d vad=%d, plc=%d,"
                       " bitrate=%d", amr_data->clock_rate,
			amr_data->vad_enabled, amr_data->plc_enabled, 
			amr_bitrates[idx][amr_data->enc_mode]));
    return PJ_SUCCESS;
}


/*
 * Close codec.
 */
static pj_status_t amr_codec_close( pjmedia_codec *codec )
{
    struct amr_data *amr_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    amr_data = (struct amr_data*) codec->codec_data;
    PJ_ASSERT_RETURN(amr_data != NULL, PJ_EINVALIDOP);

    if (amr_data->encoder) {
        if (amr_data->enc_setting.amr_nb) {
#ifdef USE_AMRNB
            Encoder_Interface_exit(amr_data->encoder);
#endif
        } else {
#ifdef USE_AMRWB
            E_IF_exit(amr_data->encoder);
#endif
        }
        amr_data->encoder = NULL;
    }

    if (amr_data->decoder) {
        if (amr_data->dec_setting.amr_nb) {
#ifdef USE_AMRNB
            Decoder_Interface_exit(amr_data->decoder);
#endif
        } else {
#ifdef USE_AMRWB
            D_IF_exit(amr_data->decoder);
#endif
        }
        amr_data->decoder = NULL;
    }
    
    TRACE_((THIS_FILE, "AMR codec closed"));
    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t amr_codec_modify( pjmedia_codec *codec, 
				     const pjmedia_codec_param *attr )
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pj_bool_t prev_vad_state;

    pj_assert(amr_data != NULL);
    pj_assert(amr_data->encoder != NULL && amr_data->decoder != NULL);

    prev_vad_state = amr_data->vad_enabled;
    amr_data->vad_enabled = (attr->setting.vad != 0);
    amr_data->plc_enabled = (attr->setting.plc != 0);

    if (amr_data->enc_setting.amr_nb &&
        prev_vad_state != amr_data->vad_enabled)
    {
	/* Reinit AMR encoder to update VAD setting */
	TRACE_((THIS_FILE, "Reiniting AMR encoder to update VAD setting."));
#ifdef USE_AMRNB
        Encoder_Interface_exit(amr_data->encoder);
        amr_data->encoder = Encoder_Interface_init(amr_data->vad_enabled);
#endif
        if (amr_data->encoder == NULL) {
	    TRACE_((THIS_FILE, "Encoder_Interface_init() failed"));
	    amr_codec_close(codec);
	    return PJMEDIA_CODEC_EFAILED;
	}
    }

    TRACE_((THIS_FILE, "AMR codec modified: vad=%d, plc=%d",
			amr_data->vad_enabled, amr_data->plc_enabled));
    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t amr_codec_parse( pjmedia_codec *codec,
				    void *pkt,
				    pj_size_t pkt_size,
				    const pj_timestamp *ts,
				    unsigned *frame_cnt,
				    pjmedia_frame frames[])
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pj_uint8_t cmr;
    pj_status_t status;
    unsigned idx = (amr_data->enc_setting.amr_nb? 0: 1);

    status = pjmedia_codec_amr_parse(pkt, pkt_size, ts, &amr_data->dec_setting,
				     frames, frame_cnt, &cmr);
    if (status != PJ_SUCCESS)
	return status;

    /* Check for Change Mode Request. */
    if (cmr < PJ_ARRAY_SIZE(amr_bitrates[idx]) && amr_data->enc_mode != cmr) {
	amr_data->enc_mode = cmr;
	TRACE_((THIS_FILE, "AMR encoder switched mode to %d (%dbps)",
                amr_data->enc_mode, 
                amr_bitrates[idx][amr_data->enc_mode]));
    }

    return PJ_SUCCESS;
}


/*
 * Encode frame.
 */
static pj_status_t amr_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    unsigned char *bitstream;
    pj_int16_t *speech;
    unsigned nsamples, samples_per_frame;
    enum {MAX_FRAMES_PER_PACKET = 16};
    pjmedia_frame frames[MAX_FRAMES_PER_PACKET];
    pj_uint8_t *p;
    unsigned i, out_size = 0, nframes = 0;
    pj_size_t payload_len;
    unsigned dtx_cnt, sid_cnt;
    pj_status_t status;
    int size;

    pj_assert(amr_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    nsamples = input->size >> 1;
    samples_per_frame = amr_data->clock_rate * FRAME_LENGTH_MS / 1000;
    PJ_ASSERT_RETURN(nsamples % samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    nframes = nsamples / samples_per_frame;
    PJ_ASSERT_RETURN(nframes <= MAX_FRAMES_PER_PACKET, 
		     PJMEDIA_CODEC_EFRMTOOSHORT);

    /* Encode the frames */
    speech = (pj_int16_t*)input->buf;
    bitstream = (unsigned char*)output->buf;
    while (nsamples >= samples_per_frame) {
        if (amr_data->enc_setting.amr_nb) {
#ifdef USE_AMRNB
            size = Encoder_Interface_Encode (amr_data->encoder,
                                             amr_data->enc_mode,
                                             speech, bitstream, 0);
#endif
        } else {
#ifdef USE_AMRWB
            size = E_IF_encode (amr_data->encoder, amr_data->enc_mode,
                                speech, bitstream, 0);
#endif
        }
	if (size == 0) {
	    output->size = 0;
	    output->buf = NULL;
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    TRACE_((THIS_FILE, "AMR encode() failed"));
	    return PJMEDIA_CODEC_EFAILED;
	}
	nsamples -= samples_per_frame;
	speech += samples_per_frame;
	bitstream += size;
	out_size += size;
	TRACE_((THIS_FILE, "AMR encode(): mode=%d, size=%d",
		amr_data->enc_mode, out_size));
    }

    /* Pack payload */
    p = (pj_uint8_t*)output->buf + output_buf_len - out_size;
    pj_memmove(p, output->buf, out_size);
    dtx_cnt = sid_cnt = 0;
    for (i = 0; i < nframes; ++i) {
	pjmedia_codec_amr_bit_info *info = (pjmedia_codec_amr_bit_info*)
					   &frames[i].bit_info;
	info->frame_type = (pj_uint8_t)((*p >> 3) & 0x0F);
	info->good_quality = (pj_uint8_t)((*p >> 2) & 0x01);
	info->mode = (pj_int8_t)amr_data->enc_mode;
	info->start_bit = 0;
	frames[i].buf = p + 1;
        if (amr_data->enc_setting.amr_nb) {
            frames[i].size = (info->frame_type <= 8)?
                             pjmedia_codec_amrnb_framelen[info->frame_type] : 0;
        } else {
            frames[i].size = (info->frame_type <= 9)?
                             pjmedia_codec_amrwb_framelen[info->frame_type] : 0;
        }
	p += frames[i].size + 1;

	/* Count the number of SID and DTX frames */
	if (info->frame_type == 15) /* DTX*/
	    ++dtx_cnt;
	else if (info->frame_type == 8) /* SID */
	    ++sid_cnt;
    }

    /* VA generates DTX frames as DTX+SID frames switching quickly and it
     * seems that the SID frames occur too often (assuming the purpose is 
     * only for keeping NAT alive?). So let's modify the behavior a bit.
     * Only an SID frame will be sent every PJMEDIA_CODEC_MAX_SILENCE_PERIOD
     * milliseconds.
     */
    if (sid_cnt + dtx_cnt == nframes) {
	pj_int32_t dtx_duration;

	dtx_duration = pj_timestamp_diff32(&amr_data->last_tx, 
					   &input->timestamp);
	if (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	    dtx_duration < PJMEDIA_CODEC_MAX_SILENCE_PERIOD*
                           amr_data->clock_rate/1000)
	{
	    output->size = 0;
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	}
    }

    payload_len = output_buf_len;

    status = pjmedia_codec_amr_pack(frames, nframes, &amr_data->enc_setting,
				    output->buf, &payload_len);
    if (status != PJ_SUCCESS) {
	output->size = 0;
	output->buf = NULL;
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	TRACE_((THIS_FILE, "Failed to pack AMR payload, status=%d", status));
	return status;
    }

    output->size = payload_len;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    amr_data->last_tx = input->timestamp;

    return PJ_SUCCESS;
}


/*
 * Decode frame.
 */
static pj_status_t amr_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct amr_data *amr_data = (struct amr_data*) codec->codec_data;
    pjmedia_frame input_;
    pjmedia_codec_amr_bit_info *info;
    unsigned out_size;
    /* AMR decoding buffer: AMR max frame size + 1 byte header. */
    unsigned char bitstream[61];

    pj_assert(amr_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    out_size = amr_data->clock_rate * FRAME_LENGTH_MS / 1000 * 2;
    if (output_buf_len < out_size)
	return PJMEDIA_CODEC_EPCMTOOSHORT;

    input_.buf = &bitstream[1];
    /* AMR max frame size */
    input_.size = (amr_data->dec_setting.amr_nb? 31: 60);
    pjmedia_codec_amr_predecode(input, &amr_data->dec_setting, &input_);
    info = (pjmedia_codec_amr_bit_info*)&input_.bit_info;

    /* VA AMR decoder requires frame info in the first byte. */
    bitstream[0] = (info->frame_type << 3) | (info->good_quality << 2);

    TRACE_((THIS_FILE, "AMR decode(): mode=%d, ft=%d, size=%d",
	    info->mode, info->frame_type, input_.size));

    /* Decode */
    if (amr_data->dec_setting.amr_nb) {
#ifdef USE_AMRNB
        Decoder_Interface_Decode(amr_data->decoder, bitstream,
                                 (pj_int16_t*)output->buf, 0);
#endif
    } else {
#ifdef USE_AMRWB
        D_IF_decode(amr_data->decoder, bitstream,
                    (pj_int16_t*)output->buf, 0);
#endif
    }

    output->size = out_size;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

#if USE_PJMEDIA_PLC
    if (amr_data->plc_enabled)
	pjmedia_plc_save(amr_data->plc, (pj_int16_t*)output->buf);
#endif

    return PJ_SUCCESS;
}


/*
 * Recover lost frame.
 */
#if USE_PJMEDIA_PLC
/*
 * Recover lost frame.
 */
static pj_status_t  amr_codec_recover( pjmedia_codec *codec,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    struct amr_data *amr_data = codec->codec_data;
    unsigned out_size = amr_data->clock_rate * FRAME_LENGTH_MS / 1000 * 2;

    TRACE_((THIS_FILE, "amr_codec_recover"));

    PJ_ASSERT_RETURN(amr_data->plc_enabled, PJ_EINVALIDOP);

    PJ_ASSERT_RETURN(output_buf_len >= out_size,  PJMEDIA_CODEC_EPCMTOOSHORT);

    pjmedia_plc_generate(amr_data->plc, (pj_int16_t*)output->buf);

    output->size = out_size;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    
    return PJ_SUCCESS;
}
#endif

#if defined(_MSC_VER) && PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS
#   if PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC
#       ifdef USE_AMRNB
#           pragma comment( lib, "libopencore-amrnb.a")
#       endif
#       ifdef USE_AMRWB
#           pragma comment( lib, "libopencore-amrwb.a")
#           pragma comment( lib, "libvo-amrwbenc.a")
#       endif
#   else
#       error Unsupported OpenCORE AMR library, fix here
#   endif
#endif

#endif

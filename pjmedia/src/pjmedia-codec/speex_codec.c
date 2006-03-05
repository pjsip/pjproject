/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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

#include <pjmedia-codec/speex.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/port.h>
#include <speex/speex.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

#define DEFAULT_QUALITY	    8
#define DEFAULT_COMPLEXITY  8


/* Prototypes for Speex factory */
static pj_status_t spx_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t spx_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t spx_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t spx_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t spx_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for Speex implementation. */
static pj_status_t  spx_codec_default_attr(pjmedia_codec *codec, 
					   pjmedia_codec_param *attr);
static pj_status_t  spx_codec_init( pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  spx_codec_open( pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  spx_codec_close( pjmedia_codec *codec );
static pj_status_t  spx_codec_get_frames( pjmedia_codec *codec,
					  void *pkt,
					  pj_size_t pkt_size,
					  unsigned *frame_cnt,
					  pjmedia_frame frames[]);
static pj_status_t  spx_codec_encode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  spx_codec_decode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);

/* Definition for Speex codec operations. */
static pjmedia_codec_op spx_op = 
{
    &spx_codec_default_attr,
    &spx_codec_init,
    &spx_codec_open,
    &spx_codec_close,
    &spx_codec_get_frames,
    &spx_codec_encode,
    &spx_codec_decode
};

/* Definition for Speex codec factory operations. */
static pjmedia_codec_factory_op spx_factory_op =
{
    &spx_test_alloc,
    &spx_default_attr,
    &spx_enum_codecs,
    &spx_alloc_codec,
    &spx_dealloc_codec
};

/* Index to Speex parameter. */
enum
{
    PARAM_NB,	/* Index for narrowband parameter.	*/
    PARAM_WB,	/* Index for wideband parameter.	*/
    PARAM_UWB,	/* Index for ultra-wideband parameter	*/
};

/* Speex default parameter */
struct speex_param
{
    int	    enabled;	/* Is this mode enabled?		*/
    const SpeexMode *mode;  /* Speex mode.			*/
    int	    pt;		/* Payload type.			*/
    unsigned clock_rate;/* Default sampling rate to be used.	*/
    int	    quality;	/* Default encoder quality to be used.	*/
    int	    complexity;	/* Default encoder complexity.		*/
    int	    samples_per_frame;	/* Samples per frame.		*/
    int	    framesize;	/* Frame size for current mode.		*/
    int	    bitrate;	/* Bit rate for current mode.		*/
};

/* Speex factory */
static struct spx_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
    pjmedia_codec	     codec_list;
    struct speex_param	     speex_param[3];

} spx_factory;

/* Speex codec private data. */
struct spx_private
{
    //pjmedia_codec_info	 info;		    /**< Codec info.		*/

    int			 param_id;	    /**< Index to speex param.	*/

    void		*enc;		    /**< Encoder state.		*/
    SpeexBits		 enc_bits;	    /**< Encoder bits.		*/
    void		*dec;		    /**< Decoder state.		*/
    SpeexBits		 dec_bits;	    /**< Decoder bits.		*/
};


/*
 * Get codec bitrate and frame size.
 */
static pj_status_t get_speex_info( struct speex_param *p )
{
    void *state;
    int tmp;

    /* Create temporary encoder */
    state = speex_encoder_init(p->mode);
    if (!state)
	return PJMEDIA_CODEC_EFAILED;

    /* Set the quality */
    speex_encoder_ctl(state, SPEEX_SET_QUALITY, &p->quality);

    /* Sampling rate. */
    speex_encoder_ctl(state, SPEEX_SET_SAMPLING_RATE, &p->clock_rate);

    /* VAD */
    tmp = 1;
    speex_encoder_ctl(state, SPEEX_SET_VAD, &tmp);

    /* Complexity. */
    speex_encoder_ctl(state, SPEEX_SET_COMPLEXITY, &p->complexity);

    /* Now get the frame size */
    speex_encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &p->samples_per_frame);

    /* Now get the the averate bitrate */
    speex_encoder_ctl(state, SPEEX_GET_BITRATE, &p->bitrate);

    /* Calculate framesize. */
    p->framesize = p->bitrate * 20 / 1000;

    /* Destroy encoder. */
    speex_encoder_destroy(state);

    return PJ_SUCCESS;
}

/*
 * Initialize and register Speex codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_init( pjmedia_endpt *endpt,
					      unsigned options,
					      int quality,
					      int complexity )
{
    pjmedia_codec_mgr *codec_mgr;
    unsigned i;
    pj_status_t status;

    if (spx_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Get defaults */
    if (quality < 0) quality = DEFAULT_QUALITY;
    if (complexity < 0) complexity = DEFAULT_COMPLEXITY;

    /* Create Speex codec factory. */
    spx_factory.base.op = &spx_factory_op;
    spx_factory.base.factory_data = NULL;
    spx_factory.endpt = endpt;

    spx_factory.pool = pjmedia_endpt_create_pool(endpt, "speex", 
						       4000, 4000);
    if (!spx_factory.pool)
	return PJ_ENOMEM;

    pj_list_init(&spx_factory.codec_list);

    /* Create mutex. */
    status = pj_mutex_create_simple(spx_factory.pool, "speex", 
				    &spx_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize default Speex parameter. */
    spx_factory.speex_param[PARAM_NB].enabled = 
	((options & PJMEDIA_SPEEX_NO_NB) == 0);
    spx_factory.speex_param[PARAM_NB].pt = 102;
    spx_factory.speex_param[PARAM_NB].mode = &speex_nb_mode;
    spx_factory.speex_param[PARAM_NB].clock_rate = 8000;
    spx_factory.speex_param[PARAM_NB].quality = quality;
    spx_factory.speex_param[PARAM_NB].complexity = complexity;

    spx_factory.speex_param[PARAM_WB].enabled = 
	((options & PJMEDIA_SPEEX_NO_WB) == 0);
    spx_factory.speex_param[PARAM_WB].pt = 103;
    spx_factory.speex_param[PARAM_WB].mode = &speex_wb_mode;
    spx_factory.speex_param[PARAM_WB].clock_rate = 16000;
    spx_factory.speex_param[PARAM_WB].quality = quality;
    spx_factory.speex_param[PARAM_WB].complexity = complexity;

    spx_factory.speex_param[PARAM_UWB].enabled = 
	((options & PJMEDIA_SPEEX_NO_UWB) == 0);
    spx_factory.speex_param[PARAM_UWB].pt = 104;
    spx_factory.speex_param[PARAM_UWB].mode = &speex_uwb_mode;
    spx_factory.speex_param[PARAM_UWB].clock_rate = 32000;
    spx_factory.speex_param[PARAM_UWB].quality = quality;
    spx_factory.speex_param[PARAM_UWB].complexity = complexity;

    /* Get codec framesize and avg bitrate for each mode. */
    for (i=0; i<PJ_ARRAY_SIZE(spx_factory.speex_param); ++i) {
	status = get_speex_info(&spx_factory.speex_param[i]);
    }

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&spx_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(spx_factory.pool);
    spx_factory.pool = NULL;
    return status;
}


/*
 * Initialize with default settings.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_init_default(pjmedia_endpt *endpt)
{
    return pjmedia_codec_speex_init(endpt, 0, -1, -1);
}

/*
 * Unregister Speex codec factory from pjmedia endpoint and deinitialize
 * the Speex codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_speex_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (spx_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    /* We don't want to deinit if there's outstanding codec. */
    pj_mutex_lock(spx_factory.mutex);
    if (!pj_list_empty(&spx_factory.codec_list)) {
	pj_mutex_unlock(spx_factory.mutex);
	return PJ_EBUSY;
    }

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(spx_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(spx_factory.pool);
	spx_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister Speex codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &spx_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(spx_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(spx_factory.pool);
    spx_factory.pool = NULL;

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t spx_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    const pj_str_t speex_tag = { "speex", 5};
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp(&info->encoding_name, &speex_tag) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    for (i=0; i<PJ_ARRAY_SIZE(spx_factory.speex_param); ++i) {
	if (info->sample_rate == spx_factory.speex_param[i].clock_rate) {
	    /* Okay, let's Speex! */
	    return PJ_SUCCESS;
	}
    }

    
    /* Unsupported, or mode is disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Generate default attribute.
 */
static pj_status_t spx_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{

    PJ_ASSERT_RETURN(factory==&spx_factory.base, PJ_EINVAL);

    pj_memset(attr, 0, sizeof(pjmedia_codec_param));
    attr->pt = id->pt;

    if (id->sample_rate <= 8000) {
	attr->sample_rate = spx_factory.speex_param[PARAM_NB].clock_rate;
	attr->avg_bps = spx_factory.speex_param[PARAM_NB].bitrate;

    } else if (id->sample_rate <= 16000) {
	attr->sample_rate = spx_factory.speex_param[PARAM_WB].clock_rate;
	attr->avg_bps = spx_factory.speex_param[PARAM_WB].bitrate;

    } else {
	/* Wow.. somebody is doing ultra-wideband. Cool...! */
	attr->sample_rate = spx_factory.speex_param[PARAM_UWB].clock_rate;
	attr->avg_bps = spx_factory.speex_param[PARAM_UWB].bitrate;
    }

    attr->pcm_bits_per_sample = 16;
    attr->ptime = 20;
    attr->pt = id->pt;

    /* Default flags. */
    attr->cng_enabled = 1;
    attr->concl_enabled = 1;
    attr->hpf_enabled = 1;
    attr->lpf_enabled =1 ;
    attr->penh_enabled =1 ;
    attr->vad_enabled = 1;

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory (i.e. only Speex!).
 */
static pj_status_t spx_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    unsigned max;
    int i;  /* Must be signed */

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;
    *count = 0;

    /*
     * We return three codecs here, and in this order:
     *	- ultra-wideband, wideband, and narrowband.
     */
    for (i=PJ_ARRAY_SIZE(spx_factory.speex_param)-1; i>=0 && *count<max; --i) {

	if (!spx_factory.speex_param[i].enabled)
	    continue;

	pj_memset(&codecs[*count], 0, sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str("speex");
	codecs[*count].pt = spx_factory.speex_param[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].sample_rate = spx_factory.speex_param[i].clock_rate;

	++*count;
    }

    return PJ_SUCCESS;
}

/*
 * Allocate a new Speex codec instance.
 */
static pj_status_t spx_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec;
    struct spx_private *spx;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &spx_factory.base, PJ_EINVAL);


    pj_mutex_lock(spx_factory.mutex);

    /* Get free nodes, if any. */
    if (!pj_list_empty(&spx_factory.codec_list)) {
	codec = spx_factory.codec_list.next;
	pj_list_erase(codec);
    } else {
	codec = pj_pool_zalloc(spx_factory.pool, 
			       sizeof(pjmedia_codec));
	PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
	codec->op = &spx_op;
	codec->factory = factory;
	codec->codec_data = pj_pool_alloc(spx_factory.pool,
					  sizeof(struct spx_private));
    }

    pj_mutex_unlock(spx_factory.mutex);

    spx = (struct spx_private*) codec->codec_data;
    spx->enc = NULL;
    spx->dec = NULL;

    if (id->sample_rate <= 8000)
	spx->param_id = PARAM_NB;
    else if (id->sample_rate <= 16000)
	spx->param_id = PARAM_WB;
    else
	spx->param_id = PARAM_UWB;

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t spx_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct spx_private *spx;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &spx_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    spx = (struct spx_private*) codec->codec_data;
    if (spx->enc != NULL || spx->dec != NULL) {
	spx_codec_close(codec);
    }

    /* Put in the free list. */
    pj_mutex_lock(spx_factory.mutex);
    pj_list_push_front(&spx_factory.codec_list, codec);
    pj_mutex_unlock(spx_factory.mutex);

    return PJ_SUCCESS;
}

/*
 * Get codec default attributes.
 */
static pj_status_t spx_codec_default_attr( pjmedia_codec *codec, 
					   pjmedia_codec_param *attr)
{
    struct spx_private *spx;
    pjmedia_codec_info info;

    spx = (struct spx_private*) codec->codec_data;

    info.encoding_name = pj_str("speex");
    info.pt = 200;  /* Don't care */
    info.sample_rate = spx_factory.speex_param[spx->param_id].clock_rate;
    info.type = PJMEDIA_TYPE_AUDIO;

    return spx_default_attr( codec->factory, &info, attr);
}

/*
 * Init codec.
 */
static pj_status_t spx_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t spx_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct spx_private *spx;
    int id, tmp;

    spx = (struct spx_private*) codec->codec_data;
    id = spx->param_id;

    /* 
     * Create and initialize encoder. 
     */
    spx->enc = speex_encoder_init(spx_factory.speex_param[id].mode);
    if (!spx->enc)
	return PJMEDIA_CODEC_EFAILED;
    speex_bits_init(&spx->enc_bits);

    /* Set the quality*/
    speex_encoder_ctl(spx->enc, SPEEX_SET_QUALITY, 
		      &spx_factory.speex_param[id].quality);

    /* Sampling rate. */
    tmp = attr->sample_rate;
    speex_encoder_ctl(spx->enc, SPEEX_SET_SAMPLING_RATE, 
		      &spx_factory.speex_param[id].clock_rate);

    /* VAD */
    tmp = attr->vad_enabled;
    speex_encoder_ctl(spx->enc, SPEEX_SET_VAD, &tmp);

    /* Complexity */
    speex_encoder_ctl(spx->enc, SPEEX_SET_BITRATE, 
		      &spx_factory.speex_param[id].complexity);

    /* Bitrate */
    speex_encoder_ctl(spx->enc, SPEEX_SET_BITRATE, 
		      &spx_factory.speex_param[id].bitrate);

    /* 
     * Create and initialize decoder. 
     */
    spx->dec = speex_decoder_init(spx_factory.speex_param[id].mode);
    if (!spx->dec) {
	spx_codec_close(codec);
	return PJMEDIA_CODEC_EFAILED;
    }
    speex_bits_init(&spx->dec_bits);

    /* Sampling rate. */
    speex_decoder_ctl(spx->dec, SPEEX_SET_SAMPLING_RATE, 
		      &spx_factory.speex_param[id].clock_rate);

    /* PENH */
    tmp = attr->penh_enabled;
    speex_decoder_ctl(spx->dec, SPEEX_SET_ENH, &tmp);

    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t spx_codec_close( pjmedia_codec *codec )
{
    struct spx_private *spx;

    spx = (struct spx_private*) codec->codec_data;

    /* Destroy encoder*/
    if (spx->enc) {
	speex_encoder_destroy( spx->enc );
	spx->enc = NULL;
	speex_bits_destroy( &spx->enc_bits );
    }

    /* Destroy decoder */
    if (spx->dec) {
	speex_decoder_destroy( spx->dec);
	spx->dec = NULL;
	speex_bits_destroy( &spx->dec_bits );
    }

    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  spx_codec_get_frames( pjmedia_codec *codec,
					  void *pkt,
					  pj_size_t pkt_size,
					  unsigned *frame_cnt,
					  pjmedia_frame frames[])
{
    struct spx_private *spx;
    unsigned speex_frame_size;
    unsigned count;

    spx = (struct spx_private*) codec->codec_data;

    speex_frame_size = spx_factory.speex_param[spx->param_id].framesize;

    /* Don't really know how to do this... */
    count = 0;
    while (pkt_size >= speex_frame_size && count < *frame_cnt) {
	frames[count].buf = pkt;
	frames[count].size = speex_frame_size;
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].timestamp.u64 = 0;

	pkt_size -= speex_frame_size;
	++count;
	pkt = ((char*)pkt) + speex_frame_size;
    }

    if (pkt_size && count < *frame_cnt) {
	frames[count].buf = pkt;
	frames[count].size = pkt_size;
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].timestamp.u64 = 0;
	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frame.
 */
static pj_status_t spx_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct spx_private *spx;
    float tmp[642]; /* 20ms at 32KHz + 2 */
    pj_int16_t *samp_in;
    unsigned i, samp_count, sz;

    spx = (struct spx_private*) codec->codec_data;

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	output->size = 0;
	output->buf = NULL;
	output->timestamp = input->timestamp;
	output->type = input->type;
	return PJ_SUCCESS;
    }

    /* Copy frame to float buffer. */
    samp_count = input->size / 2;
    pj_assert(samp_count <= PJ_ARRAY_SIZE(tmp));
    samp_in = input->buf;
    for (i=0; i<samp_count; ++i) {
	tmp[i] = samp_in[i];
    }

    /* Flush all the bits in the struct so we can encode a new frame */
    speex_bits_reset(&spx->enc_bits);

    /* Encode the frame */
    speex_encode(spx->enc, tmp, &spx->enc_bits);

    /* Check size. */
    sz = speex_bits_nbytes(&spx->enc_bits);
    pj_assert(sz <= output_buf_len);

    /* Copy the bits to an array of char that can be written */
    output->size = speex_bits_write(&spx->enc_bits, 
				    output->buf, output_buf_len);
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t spx_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct spx_private *spx;
    float tmp[642]; /* 20ms at 32KHz + 2 */
    pj_int16_t *dst_buf;
    unsigned i, count, sz;

    spx = (struct spx_private*) codec->codec_data;

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pj_memset(output->buf, 0, output_buf_len);
	output->size = 320;
	output->timestamp.u64 = input->timestamp.u64;
	output->type = PJMEDIA_FRAME_TYPE_AUDIO;
	return PJ_SUCCESS;
    }

    /* Initialization of the structure that holds the bits */
    speex_bits_init(&spx->dec_bits);

    /* Copy the data into the bit-stream struct */
    speex_bits_read_from(&spx->dec_bits, input->buf, input->size);

    /* Decode the data */
    speex_decode(spx->dec, &spx->dec_bits, tmp);

    /* Check size. */
    sz = speex_bits_nbytes(&spx->enc_bits);
    pj_assert(sz <= output_buf_len);

    /* Copy from float to short samples. */
    count = spx_factory.speex_param[spx->param_id].clock_rate * 20 / 1000;
    pj_assert((count <= output_buf_len/2) && count <= PJ_ARRAY_SIZE(tmp));
    dst_buf = output->buf;
    for (i=0; i<count; ++i) {
	dst_buf[i] = (pj_int16_t)tmp[i];
    }
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = count * 2;
    output->timestamp.u64 = input->timestamp.u64;


    return PJ_SUCCESS;
}

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
#include <pjmedia-codec/gsm.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/port.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>
#include "gsm/gsm.h"

/* Prototypes for GSM factory */
static pj_status_t gsm_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *id );
static pj_status_t gsm_default_attr( pjmedia_codec_factory *factory, 
				     const pjmedia_codec_info *id, 
				     pjmedia_codec_param *attr );
static pj_status_t gsm_enum_codecs( pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[]);
static pj_status_t gsm_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id, 
				    pjmedia_codec **p_codec);
static pj_status_t gsm_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec );

/* Prototypes for GSM implementation. */
static pj_status_t  gsm_codec_default_attr(pjmedia_codec *codec, 
					   pjmedia_codec_param *attr);
static pj_status_t  gsm_codec_init( pjmedia_codec *codec, 
				    pj_pool_t *pool );
static pj_status_t  gsm_codec_open( pjmedia_codec *codec, 
				    pjmedia_codec_param *attr );
static pj_status_t  gsm_codec_close( pjmedia_codec *codec );
static pj_status_t  gsm_codec_get_frames( pjmedia_codec *codec,
					  void *pkt,
					  pj_size_t pkt_size,
					  unsigned *frame_cnt,
					  pjmedia_frame frames[]);
static pj_status_t  gsm_codec_encode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);
static pj_status_t  gsm_codec_decode( pjmedia_codec *codec, 
				      const struct pjmedia_frame *input,
				      unsigned output_buf_len, 
				      struct pjmedia_frame *output);

/* Definition for GSM codec operations. */
static pjmedia_codec_op gsm_op = 
{
    &gsm_codec_default_attr,
    &gsm_codec_init,
    &gsm_codec_open,
    &gsm_codec_close,
    &gsm_codec_get_frames,
    &gsm_codec_encode,
    &gsm_codec_decode
};

/* Definition for GSM codec factory operations. */
static pjmedia_codec_factory_op gsm_factory_op =
{
    &gsm_test_alloc,
    &gsm_default_attr,
    &gsm_enum_codecs,
    &gsm_alloc_codec,
    &gsm_dealloc_codec
};

/* GSM factory */
static struct gsm_codec_factory
{
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;
    pjmedia_codec	     codec_list;
} gsm_codec_factory;

/* GSM codec private data. */
struct gsm_data
{
    void    *encoder;
    void    *decoder;
};



/*
 * Initialize and register GSM codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_gsm_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (gsm_codec_factory.pool != NULL)
	return PJ_SUCCESS;

    /* Create GSM codec factory. */
    gsm_codec_factory.base.op = &gsm_factory_op;
    gsm_codec_factory.base.factory_data = NULL;
    gsm_codec_factory.endpt = endpt;

    gsm_codec_factory.pool = pjmedia_endpt_create_pool(endpt, "gsm", 4000, 
						       4000);
    if (!gsm_codec_factory.pool)
	return PJ_ENOMEM;

    pj_list_init(&gsm_codec_factory.codec_list);

    /* Create mutex. */
    status = pj_mutex_create_simple(gsm_codec_factory.pool, "gsm", 
				    &gsm_codec_factory.mutex);
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
						&gsm_codec_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(gsm_codec_factory.pool);
    gsm_codec_factory.pool = NULL;
    return status;
}



/*
 * Unregister GSM codec factory from pjmedia endpoint and deinitialize
 * the GSM codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_gsm_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (gsm_codec_factory.pool == NULL)
	return PJ_SUCCESS;

    /* We don't want to deinit if there's outstanding codec. */
    pj_mutex_lock(gsm_codec_factory.mutex);
    if (!pj_list_empty(&gsm_codec_factory.codec_list)) {
	pj_mutex_unlock(gsm_codec_factory.mutex);
	return PJ_EBUSY;
    }

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(gsm_codec_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(gsm_codec_factory.pool);
	gsm_codec_factory.pool = NULL;
	return PJ_EINVALIDOP;
    }

    /* Unregister GSM codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &gsm_codec_factory.base);
    
    /* Destroy mutex. */
    pj_mutex_destroy(gsm_codec_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(gsm_codec_factory.pool);
    gsm_codec_factory.pool = NULL;

    return status;
}

/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t gsm_test_alloc( pjmedia_codec_factory *factory, 
				   const pjmedia_codec_info *info )
{
    PJ_UNUSED_ARG(factory);

    /* Check payload type. */
    if (info->pt != PJMEDIA_RTP_PT_GSM)
	return PJMEDIA_CODEC_EUNSUP;

    /* Ignore the rest, since it's static payload type. */

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t gsm_default_attr (pjmedia_codec_factory *factory, 
				      const pjmedia_codec_info *id, 
				      pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(id);

    pj_memset(attr, 0, sizeof(pjmedia_codec_param));
    attr->sample_rate = 8000;
    attr->avg_bps = 13200;
    attr->pcm_bits_per_sample = 16;
    attr->ptime = 20;
    attr->pt = PJMEDIA_RTP_PT_GSM;

    /* Default all flag bits disabled. */

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory (i.e. only GSM!).
 */
static pj_status_t gsm_enum_codecs(pjmedia_codec_factory *factory, 
				    unsigned *count, 
				    pjmedia_codec_info codecs[])
{
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    pj_memset(&codecs[0], 0, sizeof(pjmedia_codec_info));
    codecs[0].encoding_name = pj_str("GSM");
    codecs[0].pt = PJMEDIA_RTP_PT_GSM;
    codecs[0].type = PJMEDIA_TYPE_AUDIO;
    codecs[0].sample_rate = 8000;

    *count = 1;

    return PJ_SUCCESS;
}

/*
 * Allocate a new GSM codec instance.
 */
static pj_status_t gsm_alloc_codec( pjmedia_codec_factory *factory, 
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pjmedia_codec *codec;
    struct gsm_data *gsm_data;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &gsm_codec_factory.base, PJ_EINVAL);


    pj_mutex_lock(gsm_codec_factory.mutex);

    /* Get free nodes, if any. */
    if (!pj_list_empty(&gsm_codec_factory.codec_list)) {
	codec = gsm_codec_factory.codec_list.next;
	pj_list_erase(codec);
    } else {
	codec = pj_pool_zalloc(gsm_codec_factory.pool, 
			       sizeof(pjmedia_codec));
	PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
	codec->op = &gsm_op;
	codec->factory = factory;

	gsm_data = pj_pool_zalloc(gsm_codec_factory.pool, 
				  sizeof(struct gsm_data));
	codec->codec_data = gsm_data;
    }

    pj_mutex_unlock(gsm_codec_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}

/*
 * Free codec.
 */
static pj_status_t gsm_dealloc_codec( pjmedia_codec_factory *factory, 
				      pjmedia_codec *codec )
{
    struct gsm_data *gsm_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &gsm_codec_factory.base, PJ_EINVAL);

    gsm_data = codec->codec_data;

    /* Close codec, if it's not closed. */
    gsm_codec_close(codec);

    /* Put in the free list. */
    pj_mutex_lock(gsm_codec_factory.mutex);
    pj_list_push_front(&gsm_codec_factory.codec_list, codec);
    pj_mutex_unlock(gsm_codec_factory.mutex);

    return PJ_SUCCESS;
}

/*
 * Get codec default attributes.
 */
static pj_status_t gsm_codec_default_attr( pjmedia_codec *codec, 
					   pjmedia_codec_param *attr)
{
    return gsm_default_attr( codec->factory, NULL, attr);
}

/*
 * Init codec.
 */
static pj_status_t gsm_codec_init( pjmedia_codec *codec, 
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t gsm_codec_open( pjmedia_codec *codec, 
				   pjmedia_codec_param *attr )
{
    struct gsm_data *gsm_data = codec->codec_data;

    pj_assert(gsm_data != NULL);
    pj_assert(gsm_data->encoder == NULL && gsm_data->decoder == NULL);

    PJ_UNUSED_ARG(attr);

    gsm_data->encoder = gsm_create();
    if (!gsm_data->encoder)
	return PJMEDIA_CODEC_EFAILED;

    gsm_data->decoder = gsm_create();
    if (!gsm_data->decoder)
	return PJMEDIA_CODEC_EFAILED;

    return PJ_SUCCESS;
}

/*
 * Close codec.
 */
static pj_status_t gsm_codec_close( pjmedia_codec *codec )
{
    struct gsm_data *gsm_data = codec->codec_data;

    pj_assert(gsm_data != NULL);

    if (gsm_data->encoder) {
	gsm_destroy(gsm_data->encoder);
	gsm_data->encoder = NULL;
    }
    if (gsm_data->decoder) {
	gsm_destroy(gsm_data->decoder);
	gsm_data->decoder = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  gsm_codec_get_frames( pjmedia_codec *codec,
					  void *pkt,
					  pj_size_t pkt_size,
					  unsigned *frame_cnt,
					  pjmedia_frame frames[])
{
    unsigned count = 0;

    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    while (pkt_size >= 33 && count < *frame_cnt) {
	frames[0].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[0].buf = pkt;
	frames[0].size = 33;

	pkt = ((char*)pkt) + 33;
	pkt_size -= 33;

	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frame.
 */
static pj_status_t gsm_codec_encode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct gsm_data *gsm_data = codec->codec_data;

    pj_assert(gsm_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    if (output_buf_len < 33)
	return PJMEDIA_CODEC_EFRMTOOSHORT;

    if (input->size < 320)
	return PJMEDIA_CODEC_EPCMTOOSHORT;

    gsm_encode(gsm_data->encoder, (short*)input->buf, 
	       (unsigned char*)output->buf);

    output->size = 33;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t gsm_codec_decode( pjmedia_codec *codec, 
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len, 
				     struct pjmedia_frame *output)
{
    struct gsm_data *gsm_data = codec->codec_data;

    pj_assert(gsm_data != NULL);
    PJ_ASSERT_RETURN(input && output, PJ_EINVAL);

    if (output_buf_len < 320)
	return PJMEDIA_CODEC_EPCMTOOSHORT;

    if (input->size < 33)
	return PJMEDIA_CODEC_EFRMTOOSHORT;

    gsm_decode(gsm_data->decoder, 
	       (unsigned char*)input->buf, 
	       (short*)output->buf);

    output->size = 320;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}

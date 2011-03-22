/* $Id$ */
/* 
 * Copyright (C) 2010 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/ffmpeg_codecs.h>
#include <pjmedia-codec/h263_packetizer.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>


/*
 * Only build this file if PJMEDIA_HAS_FFMPEG_CODEC != 0
 */
#if defined(PJMEDIA_HAS_FFMPEG_CODEC) && PJMEDIA_HAS_FFMPEG_CODEC != 0

#define THIS_FILE   "ffmpeg_codecs.c"

#include "../pjmedia/ffmpeg_util.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>


#define PJMEDIA_FORMAT_FFMPEG_UNKNOWN  PJMEDIA_FORMAT_PACK('f','f','0','0');


/* Prototypes for FFMPEG codecs factory */
static pj_status_t ffmpeg_test_alloc( pjmedia_vid_codec_factory *factory, 
				      const pjmedia_vid_codec_info *id );
static pj_status_t ffmpeg_default_attr( pjmedia_vid_codec_factory *factory, 
				        const pjmedia_vid_codec_info *info, 
				        pjmedia_vid_codec_param *attr );
static pj_status_t ffmpeg_enum_codecs( pjmedia_vid_codec_factory *factory, 
				       unsigned *count, 
				       pjmedia_vid_codec_info codecs[]);
static pj_status_t ffmpeg_alloc_codec( pjmedia_vid_codec_factory *factory, 
				       const pjmedia_vid_codec_info *info, 
				       pjmedia_vid_codec **p_codec);
static pj_status_t ffmpeg_dealloc_codec( pjmedia_vid_codec_factory *factory, 
				         pjmedia_vid_codec *codec );

/* Prototypes for FFMPEG codecs implementation. */
static pj_status_t  ffmpeg_codec_init( pjmedia_vid_codec *codec, 
				       pj_pool_t *pool );
static pj_status_t  ffmpeg_codec_open( pjmedia_vid_codec *codec, 
				       pjmedia_vid_codec_param *attr );
static pj_status_t  ffmpeg_codec_close( pjmedia_vid_codec *codec );
static pj_status_t  ffmpeg_codec_modify(pjmedia_vid_codec *codec, 
				        const pjmedia_vid_codec_param *attr );
static pj_status_t  ffmpeg_codec_get_param(pjmedia_vid_codec *codec,
					   pjmedia_vid_codec_param *param);
static pj_status_t  ffmpeg_packetize ( pjmedia_vid_codec *codec,
                                       pj_uint8_t *buf,
                                       pj_size_t buf_len,
                                       unsigned *pos,
                                       const pj_uint8_t **payload,
                                       pj_size_t *payload_len);
static pj_status_t  ffmpeg_unpacketize(pjmedia_vid_codec *codec,
                                       const pj_uint8_t *payload,
                                       pj_size_t   payload_len,
                                       pj_uint8_t *buf,
                                       pj_size_t  *buf_len);
static pj_status_t  ffmpeg_codec_encode( pjmedia_vid_codec *codec, 
				         const pjmedia_frame *input,
				         unsigned output_buf_len, 
				         pjmedia_frame *output);
static pj_status_t  ffmpeg_codec_decode( pjmedia_vid_codec *codec, 
				         const pjmedia_frame *input,
				         unsigned output_buf_len, 
				         pjmedia_frame *output);
static pj_status_t  ffmpeg_codec_recover( pjmedia_vid_codec *codec, 
				          unsigned output_buf_len, 
				          pjmedia_frame *output);

/* Definition for FFMPEG codecs operations. */
static pjmedia_vid_codec_op ffmpeg_op = 
{
    &ffmpeg_codec_init,
    &ffmpeg_codec_open,
    &ffmpeg_codec_close,
    &ffmpeg_codec_modify,
    &ffmpeg_codec_get_param,
    &ffmpeg_packetize,
    &ffmpeg_unpacketize,
    &ffmpeg_codec_encode,
    &ffmpeg_codec_decode,
    NULL //&ffmpeg_codec_recover
};

/* Definition for FFMPEG codecs factory operations. */
static pjmedia_vid_codec_factory_op ffmpeg_factory_op =
{
    &ffmpeg_test_alloc,
    &ffmpeg_default_attr,
    &ffmpeg_enum_codecs,
    &ffmpeg_alloc_codec,
    &ffmpeg_dealloc_codec
};


/* FFMPEG codecs factory */
static struct ffmpeg_factory {
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr	*mgr;
    pj_pool_factory             *pf;
    pj_pool_t		        *pool;
    pj_mutex_t		        *mutex;
} ffmpeg_factory;


typedef struct ffmpeg_codec_desc ffmpeg_codec_desc;

/* ITU resolution ID */
typedef enum itu_res_id {
    ITU_RES_SQCIF,
    ITU_RES_QCIF,
    ITU_RES_CIF,
    ITU_RES_4CIF,
    ITU_RES_16CIF,
    ITU_RES_CUSTOM,
} itu_res_id;

/* ITU resolution definition */
struct itu_res {
    itu_res_id		id;
    pj_str_t		name;    
    pjmedia_rect_size	size;
} itu_res_def [] =
{
    {ITU_RES_16CIF,	{"16CIF",5},    {1408,1142}},
    {ITU_RES_4CIF,	{"4CIF",4},     {704,576}},
    {ITU_RES_CIF,	{"CIF",3},      {352,288}},
    {ITU_RES_QCIF,	{"QCIF",4},	{176,144}},
    {ITU_RES_SQCIF,	{"SQCIF",5},    {88,72}},
    {ITU_RES_CUSTOM,	{"CUSTOM",6},   {0,0}},
};

/* FFMPEG codecs private data. */
typedef struct ffmpeg_private {
    const ffmpeg_codec_desc	    *desc;
    pjmedia_vid_codec_param	     param;	/**< Codec param	    */
    pj_pool_t			    *pool;	/**< Pool for each instance */
    pj_timestamp		     last_tx;   /**< Timestamp of last 
						     transmit		    */

    /* Format info and apply format param */
    const pjmedia_video_format_info *enc_vfi;
    pjmedia_video_apply_fmt_param    enc_vafp;
    const pjmedia_video_format_info *dec_vfi;
    pjmedia_video_apply_fmt_param    dec_vafp;

    /* The ffmpeg codec states. */
    AVCodec			    *enc;
    AVCodec			    *dec;
    AVCodecContext		    *enc_ctx;
    AVCodecContext		    *dec_ctx;

    /* The ffmpeg decoder cannot set the output format, so format conversion
     * may be needed for post-decoding.
     */
    enum PixelFormat		     expected_dec_fmt;
						/**< expected output format of 
						     ffmpeg decoder	    */
} ffmpeg_private;


typedef pj_status_t (*func_packetize)	(pj_uint8_t *buf,
					 pj_size_t buf_len,
					 unsigned *pos,
					 int max_payload_len,
					 const pj_uint8_t **payload,
					 pj_size_t *payload_len);

typedef pj_status_t (*func_unpacketize)	(const pj_uint8_t *payload,
					 pj_size_t   payload_len,
					 pj_uint8_t *bits,
					 pj_size_t  *bits_len);

typedef pj_status_t (*func_parse_fmtp)	(ffmpeg_private *ff);

/* FFMPEG codec info */
struct ffmpeg_codec_desc {
    /* Predefined info */
    pjmedia_vid_codec_info       info;
    pjmedia_format_id		 base_fmt_id;
    pj_uint32_t			 avg_bps;
    pj_uint32_t			 max_bps;
    func_packetize		 packetize;
    func_unpacketize		 unpacketize;
    func_parse_fmtp		 parse_fmtp;
    pjmedia_codec_fmtp		 dec_fmtp;

    /* Init time defined info */
    pj_bool_t			 enabled;
    AVCodec                     *enc;
    AVCodec                     *dec;
};

/* H263 packetizer */
static pj_status_t h263_packetize(pj_uint8_t *buf,
				  pj_size_t buf_len,
				  unsigned *pos,
				  int max_payload_len,
				  const pj_uint8_t **payload,
				  pj_size_t *payload_len)
{
    return pjmedia_h263_packetize(buf, buf_len, pos, max_payload_len, 
				  payload, payload_len);
}

/* H263 unpacketizer */
static pj_status_t h263_unpacketize(const pj_uint8_t *payload,
				    pj_size_t   payload_len,
				    pj_uint8_t *bits,
				    pj_size_t  *bits_len)
{
    return pjmedia_h263_unpacketize(payload, payload_len, bits, bits_len);
}

/* H263 fmtp parser */
static pj_status_t h263_parse_fmtp(ffmpeg_private *ff);


/* Internal codec info */
ffmpeg_codec_desc codec_desc[] =
{
    {
	{PJMEDIA_FORMAT_H263P,	{"H263-1998",9},    PJMEDIA_RTP_PT_H263P},
	PJMEDIA_FORMAT_H263,	1000000,    2000000,
	&h263_packetize, &h263_unpacketize, &h263_parse_fmtp,
	{2, { {{"CIF",3}, {"2",1}}, {{"QCIF",4}, {"1",1}}, } },
    },
    {
	{PJMEDIA_FORMAT_H263,	{"H263",4},	    PJMEDIA_RTP_PT_H263},
    },
    {
	{PJMEDIA_FORMAT_H264,	{"H264",4},	    PJMEDIA_RTP_PT_H264},
    },
    {
	{PJMEDIA_FORMAT_H261,	{"H261",4},	    PJMEDIA_RTP_PT_H261},
    },
    {
	{PJMEDIA_FORMAT_MJPEG,	{"JPEG",4},	    PJMEDIA_RTP_PT_JPEG},
    },
    {
	{PJMEDIA_FORMAT_MPEG4,	{"MP4V",4}},
    },
    {
	{PJMEDIA_FORMAT_XVID,	{"XVID",4}},
	PJMEDIA_FORMAT_MPEG4,
    },
};

/* Parse fmtp value for custom resolution, e.g: "CUSTOM=800,600,2" */
static pj_status_t parse_fmtp_itu_custom_res(const pj_str_t *fmtp_val,
					     pjmedia_rect_size *size,
					     unsigned *mpi)
{
    const char *p, *p_end;
    pj_str_t token;
    unsigned long val[3] = {0};
    unsigned i = 0;

    p = token.ptr = fmtp_val->ptr;
    p_end = p + fmtp_val->slen;

    while (p<=p_end && i<PJ_ARRAY_SIZE(val)) {
	if (*p==',' || p==p_end) {
	    token.slen = (char*)p - token.ptr;
	    val[i++] = pj_strtoul(&token);
	    token.ptr = (char*)p+1;
	}
	++p;
    }

    if (!val[0] || !val[1])
	return PJ_ETOOSMALL;

    if (val[2]<1 || val[2]>32)
	return PJ_EINVAL;

    size->w = val[0];
    size->h = val[1];
    *mpi = val[2];
    return PJ_SUCCESS;
}

#define CALC_ITU_CUSTOM_RES_SCORE(size, mpi) ((size)->w * (size)->h / mpi)

/* ITU codec capabilities */
typedef struct itu_cap
{
    /* Lowest MPI for each non-custom resolution */
    unsigned		lowest_mpi[PJ_ARRAY_SIZE(itu_res_def)];
    /* For custom resolution, we use maximum processing score */
    unsigned		custom_res_max_score;
} itu_cap;


static pj_status_t load_itu_cap(const pjmedia_codec_fmtp *fmtp,
				itu_cap *cap)
{
    unsigned i, j;
    unsigned min_mpi = 0;

    /* Get Minimum Picture Interval (MPI) for each resolution. If a resolution
     * has no MPI setting in fmtp, the MPI setting is derived from the higher
     * resolution.
     */
    for (i=0; i<PJ_ARRAY_SIZE(itu_res_def); ++i) {

	/* Init lowest MPI */
	cap->lowest_mpi[i] = min_mpi? min_mpi:1;

	for (j=0; j<fmtp->cnt; ++j) {
	    if (pj_stricmp(&fmtp->param[j].name, &itu_res_def[i].name)==0) {
		pjmedia_rect_size size;
		unsigned mpi;
		unsigned score;

		if (i != ITU_RES_CUSTOM) {
		    size = itu_res_def[i].size;
		    mpi = pj_strtoul(&fmtp->param[j].val);
		    if (min_mpi)
			min_mpi = PJ_MIN(mpi, min_mpi);
		    else
			min_mpi = mpi;

		    /* Update the lowest MPI for this resolution */
		    cap->lowest_mpi[i] = min_mpi;

		    /* Also update the processing score for the custom 
		     * resolution.
		     */
		    score = CALC_ITU_CUSTOM_RES_SCORE(&size, mpi);
		    cap->custom_res_max_score = 
				    PJ_MAX(score, cap->custom_res_max_score);
		} else {
		    

		    if (parse_fmtp_itu_custom_res(&fmtp->param[j].val, 
						  &size, &mpi) == PJ_SUCCESS)
		    {
			score = CALC_ITU_CUSTOM_RES_SCORE(&size, mpi);
			cap->custom_res_max_score = 
				    PJ_MAX(score, cap->custom_res_max_score);
		    }
		}
	    }
	}
    }

    return PJ_SUCCESS;
}

/* H263 fmtp parser */
static pj_status_t h263_parse_fmtp(ffmpeg_private *ff)
{
    pjmedia_dir dir;
    pj_status_t status;

    dir = ff->param.dir;

    if (ff->param.dir & PJMEDIA_DIR_ENCODING) {
	pjmedia_vid_codec_param param_ref;
	pjmedia_codec_fmtp *fmtp_rem, *fmtp_ref;
	itu_cap local_cap;
	pjmedia_rect_size size = {0};
	unsigned mpi = 0;
	pj_bool_t got_good_res = PJ_FALSE;
	pj_bool_t has_prefered_res = PJ_FALSE;
	unsigned i, j;

	fmtp_rem = &ff->param.enc_fmtp;
	dir &= ~PJMEDIA_DIR_ENCODING;

	/* Get default fmtp setting as the reference for local capabilities */
	status = pjmedia_vid_codec_mgr_get_default_param(
			ffmpeg_factory.mgr, &ff->desc->info, &param_ref);
	fmtp_ref = (status==PJ_SUCCESS)? &param_ref.enc_fmtp : fmtp_rem;

	/* Load default local capabilities */
	status = load_itu_cap(fmtp_ref, &local_cap);
	pj_assert(status == PJ_SUCCESS);

	/* Negotiate resolution and MPI */
	for (i=0; i<fmtp_rem->cnt && !got_good_res; ++i)
	{
	    for (j=0; j<PJ_ARRAY_SIZE(itu_res_def) && !got_good_res; ++j)
	    {
		if (pj_stricmp(&fmtp_rem->param[i].name, &itu_res_def[j].name))
		    continue;

		has_prefered_res = PJ_TRUE;
		if (j == ITU_RES_CUSTOM) {
		    unsigned score;

		    if (parse_fmtp_itu_custom_res(&fmtp_rem->param[i].val, 
						  &size, &mpi) != PJ_SUCCESS)
		    {
			/* Invalid custom resolution format, skip this 
			 * custom resolution
			 */
			break;
		    }

		    score = CALC_ITU_CUSTOM_RES_SCORE(&size, mpi);
		    if (score <= local_cap.custom_res_max_score)
			got_good_res = PJ_TRUE;
		} else {
		    mpi = pj_strtoul(&fmtp_rem->param[i].val);
		    if (mpi>=1 && mpi<=32 && mpi>=local_cap.lowest_mpi[j]) {
			got_good_res = PJ_TRUE;
			size = itu_res_def[j].size;
		    }
		}
	    }
	}

	if (has_prefered_res) {
	    if (got_good_res) {
		pjmedia_video_format_detail *vfd;

		/* Apply this size & MPI */
		vfd = pjmedia_format_get_video_format_detail(&ff->param.enc_fmt,
							     PJ_TRUE);
		vfd->size = size;
		vfd->fps.num = 30000;
		vfd->fps.denum = 1001 * mpi;
		got_good_res = PJ_TRUE;

		PJ_TODO(NOTIFY_APP_ABOUT_THIS_NEW_ENCODING_FORMAT);
	    } else {
		return PJMEDIA_EBADFMT;
	    }
	}
    }

    return PJ_SUCCESS;
}



static const ffmpeg_codec_desc* find_codec_desc_by_info(
			const pjmedia_vid_codec_info *info)
{
    int i;

    for (i=0; i<PJ_ARRAY_SIZE(codec_desc); ++i) {
	ffmpeg_codec_desc *desc = &codec_desc[i];

	if (desc->enabled &&
	    (desc->info.fmt_id == info->fmt_id) &&
            ((desc->info.dir & info->dir) == info->dir) &&
            pj_stricmp(&desc->info.encoding_name, &info->encoding_name)==0)
        {
            return desc;
        }
    }

    return NULL;
}


static int find_codec_idx_by_fmt_id(pjmedia_format_id fmt_id)
{
    int i;
    for (i=0; i<PJ_ARRAY_SIZE(codec_desc); ++i) {
	if (codec_desc[i].info.fmt_id == fmt_id)
	    return i;
    }

    return -1;
}


/*
 * Initialize and register FFMPEG codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_ffmpeg_init(pjmedia_vid_codec_mgr *mgr,
                                              pj_pool_factory *pf)
{
    pj_pool_t *pool;
    AVCodec *c;
    pj_status_t status;
    unsigned i;

    if (ffmpeg_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create FFMPEG codec factory. */
    ffmpeg_factory.base.op = &ffmpeg_factory_op;
    ffmpeg_factory.base.factory_data = NULL;
    ffmpeg_factory.mgr = mgr;
    ffmpeg_factory.pf = pf;

    pool = pj_pool_create(pf, "ffmpeg codec factory", 256, 256, NULL);
    if (!pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(pool, "ffmpeg codec factory", 
				    &ffmpeg_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    avcodec_init();
    avcodec_register_all();
    av_log_set_level(AV_LOG_ERROR);

    /* Enum FFMPEG codecs */
    for (c=av_codec_next(NULL); c; c=av_codec_next(c)) 
    {
        ffmpeg_codec_desc *desc;
	pjmedia_format_id fmt_id;
	int codec_info_idx;
        
        if (c->type != CODEC_TYPE_VIDEO)
            continue;

        /* Video encoder and decoder are usually implemented in separate
         * AVCodec instances. While the codec attributes (e.g: raw formats,
	 * supported fps) are in the encoder.
         */

	//PJ_LOG(3, (THIS_FILE, "%s", c->name));
	status = CodecID_to_pjmedia_format_id(c->id, &fmt_id);
	/* Skip if format ID is unknown */
	if (status != PJ_SUCCESS)
	    continue;

	codec_info_idx = find_codec_idx_by_fmt_id(fmt_id);
	/* Skip if codec is unwanted by this wrapper (not listed in 
	 * the codec info array)
	 */
	if (codec_info_idx < 0)
	    continue;

	desc = &codec_desc[codec_info_idx];

	/* Skip duplicated codec implementation */
	if ((c->encode && (desc->info.dir & PJMEDIA_DIR_ENCODING)) ||
	    (c->decode && (desc->info.dir & PJMEDIA_DIR_DECODING)))
	{
	    continue;
	}

	/* Get raw/decoded format ids in the encoder */
	if (c->pix_fmts && c->encode) {
	    pjmedia_format_id raw_fmt[PJMEDIA_VID_CODEC_MAX_DEC_FMT_CNT];
	    unsigned raw_fmt_cnt = 0;
	    unsigned raw_fmt_cnt_should_be = 0;
	    const enum PixelFormat *p = c->pix_fmts;

	    for(;(p && *p != -1) &&
		 (raw_fmt_cnt < PJMEDIA_VID_CODEC_MAX_DEC_FMT_CNT);
		 ++p)
	    {
		pjmedia_format_id fmt_id;

		raw_fmt_cnt_should_be++;
		status = PixelFormat_to_pjmedia_format_id(*p, &fmt_id);
		if (status != PJ_SUCCESS) {
		    PJ_LOG(6, (THIS_FILE, "Unrecognized ffmpeg pixel "
			       "format %d", *p));
		    continue;
		}
		raw_fmt[raw_fmt_cnt++] = fmt_id;
	    }

	    if (raw_fmt_cnt == 0) {
		PJ_LOG(5, (THIS_FILE, "No recognized raw format "
				      "for codec [%s/%s], codec ignored",
				      c->name, c->long_name));
		/* Skip this encoder */
		continue;
	    }

	    if (raw_fmt_cnt < raw_fmt_cnt_should_be) {
		PJ_LOG(6, (THIS_FILE, "Codec [%s/%s] have %d raw formats, "
				      "recognized only %d raw formats",
				      c->name, c->long_name,
				      raw_fmt_cnt_should_be, raw_fmt_cnt));
	    }

	    desc->info.dec_fmt_id_cnt = raw_fmt_cnt;
	    pj_memcpy(desc->info.dec_fmt_id, raw_fmt, 
		      sizeof(raw_fmt[0])*raw_fmt_cnt);
	}

	/* Get supported framerates */
	if (c->supported_framerates) {
	    const AVRational *fr = c->supported_framerates;
	    while ((fr->num != 0 || fr->den != 0) && 
		   desc->info.fps_cnt < PJMEDIA_VID_CODEC_MAX_FPS_CNT)
	    {
		desc->info.fps[desc->info.fps_cnt].num = fr->num;
		desc->info.fps[desc->info.fps_cnt].denum = fr->den;
		++desc->info.fps_cnt;
		++fr;
	    }
	}

	/* Get ffmpeg encoder instance */
        if (c->encode && !desc->enc) {
            desc->info.dir |= PJMEDIA_DIR_ENCODING;
            desc->enc = c;
        }
	
	/* Get ffmpeg decoder instance */
        if (c->decode && !desc->dec) {
            desc->info.dir |= PJMEDIA_DIR_DECODING;
            desc->dec = c;
        }

	/* Enable this codec when any ffmpeg codec instance are recognized
	 * and the supported raw formats info has been collected.
	 */
	if ((desc->dec || desc->enc) && desc->info.dec_fmt_id_cnt)
	{
	    desc->enabled = PJ_TRUE;
	}

	/* Normalize default value of clock rate */
	if (desc->info.clock_rate == 0)
	    desc->info.clock_rate = 90000;

	/* Set RTP packetization support flag in the codec info */
	desc->info.has_rtp_pack = (desc->packetize != NULL) &&
				  (desc->unpacketize != NULL);
    }

    /* Init unassigned encoder/decoder description from base codec */
    for (i = 0; i < PJ_ARRAY_SIZE(codec_desc); ++i) {
	ffmpeg_codec_desc *desc = &codec_desc[i];

	if (desc->base_fmt_id && (!desc->dec || !desc->enc)) {
	    ffmpeg_codec_desc *base_desc = NULL;
	    int base_desc_idx;
	    pjmedia_dir copied_dir = PJMEDIA_DIR_NONE;

	    base_desc_idx = find_codec_idx_by_fmt_id(desc->base_fmt_id);
	    if (base_desc_idx != -1)
		base_desc = &codec_desc[base_desc_idx];
	    if (!base_desc || !base_desc->enabled)
		continue;

	    /* Copy description from base codec */
	    if (!desc->info.dec_fmt_id_cnt) {
		desc->info.dec_fmt_id_cnt = base_desc->info.dec_fmt_id_cnt;
		pj_memcpy(desc->info.dec_fmt_id, base_desc->info.dec_fmt_id, 
			  sizeof(pjmedia_format_id)*desc->info.dec_fmt_id_cnt);
	    }
	    if (!desc->info.fps_cnt) {
		desc->info.fps_cnt = base_desc->info.fps_cnt;
		pj_memcpy(desc->info.fps, base_desc->info.fps, 
			  sizeof(desc->info.fps[0])*desc->info.fps_cnt);
	    }
	    if (!desc->info.clock_rate) {
		desc->info.clock_rate = base_desc->info.clock_rate;
	    }
	    if (!desc->dec && base_desc->dec) {
		copied_dir |= PJMEDIA_DIR_DECODING;
		desc->dec = base_desc->dec;
	    }
	    if (!desc->enc && base_desc->enc) {
		copied_dir |= PJMEDIA_DIR_ENCODING;
		desc->enc = base_desc->enc;
	    }

	    desc->info.dir |= copied_dir;
	    desc->enabled = (desc->info.dir != PJMEDIA_DIR_NONE);

	    if (copied_dir != PJMEDIA_DIR_NONE) {
		const char *dir_name[] = {NULL, "encoder", "decoder", "codec"};
		PJ_LOG(5, (THIS_FILE, "The %.*s %s is using base codec (%.*s)",
			   desc->info.encoding_name.slen,
			   desc->info.encoding_name.ptr,
			   dir_name[copied_dir],
			   base_desc->info.encoding_name.slen,
			   base_desc->info.encoding_name.ptr));
	    }
        }
    }

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr, 
						    &ffmpeg_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    ffmpeg_factory.pool = pool;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(pool);
    return status;
}

/*
 * Unregister FFMPEG codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_ffmpeg_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (ffmpeg_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    pj_mutex_lock(ffmpeg_factory.mutex);

    /* Unregister FFMPEG codecs factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(ffmpeg_factory.mgr,
						      &ffmpeg_factory.base);

    /* Destroy mutex. */
    pj_mutex_destroy(ffmpeg_factory.mutex);

    /* Destroy pool. */
    pj_pool_release(ffmpeg_factory.pool);
    ffmpeg_factory.pool = NULL;

    return status;
}


/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t ffmpeg_test_alloc( pjmedia_vid_codec_factory *factory, 
				      const pjmedia_vid_codec_info *info )
{
    const ffmpeg_codec_desc *desc;

    PJ_ASSERT_RETURN(factory==&ffmpeg_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info, PJ_EINVAL);

    desc = find_codec_desc_by_info(info);
    if (!desc) {
        return PJMEDIA_CODEC_EUNSUP;
    }

    return PJ_SUCCESS;
}

/*
 * Generate default attribute.
 */
static pj_status_t ffmpeg_default_attr( pjmedia_vid_codec_factory *factory, 
				        const pjmedia_vid_codec_info *info, 
				        pjmedia_vid_codec_param *attr )
{
    const ffmpeg_codec_desc *desc;

    PJ_ASSERT_RETURN(factory==&ffmpeg_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    desc = find_codec_desc_by_info(info);
    if (!desc) {
        return PJMEDIA_CODEC_EUNSUP;
    }

    pj_bzero(attr, sizeof(pjmedia_vid_codec_param));

    /* Direction */
    attr->dir = desc->info.dir;

    /* Encoded format */
    pjmedia_format_init_video(&attr->enc_fmt, desc->info.fmt_id,
                              352, 288, 30000, 1001);

    /* Decoded format */
    pjmedia_format_init_video(&attr->dec_fmt, desc->info.dec_fmt_id[0],
                              352, 288, 30000, 1001);

    /* Decoding fmtp */
    attr->dec_fmtp = desc->dec_fmtp;

    /* Bitrate */
    attr->enc_fmt.det.vid.avg_bps = desc->avg_bps;
    attr->enc_fmt.det.vid.max_bps = desc->max_bps;

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t ffmpeg_enum_codecs( pjmedia_vid_codec_factory *factory,
				       unsigned *count, 
				       pjmedia_vid_codec_info codecs[])
{
    unsigned i, max_cnt;

    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ffmpeg_factory.base, PJ_EINVAL);

    max_cnt = PJ_MIN(*count, PJ_ARRAY_SIZE(codec_desc));
    *count = 0;

    for (i=0; i<max_cnt; ++i) {
	if (codec_desc[i].enabled) {
	    pj_memcpy(&codecs[*count], &codec_desc[i].info, 
		      sizeof(pjmedia_vid_codec_info));
	    (*count)++;
	}
    }

    return PJ_SUCCESS;
}

/*
 * Allocate a new codec instance.
 */
static pj_status_t ffmpeg_alloc_codec( pjmedia_vid_codec_factory *factory, 
				       const pjmedia_vid_codec_info *info,
				       pjmedia_vid_codec **p_codec)
{
    ffmpeg_private *ff;
    const ffmpeg_codec_desc *desc;
    pjmedia_vid_codec *codec;
    pj_pool_t *pool = NULL;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(factory && info && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ffmpeg_factory.base, PJ_EINVAL);

    desc = find_codec_desc_by_info(info);
    if (!desc) {
        return PJMEDIA_CODEC_EUNSUP;
    }

    /* Create pool for codec instance */
    pool = pj_pool_create(ffmpeg_factory.pf, "ffmpeg codec", 512, 512, NULL);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    if (!codec) {
        status = PJ_ENOMEM;
        goto on_error;
    }
    codec->op = &ffmpeg_op;
    codec->factory = factory;
    ff = PJ_POOL_ZALLOC_T(pool, ffmpeg_private);
    if (!ff) {
        status = PJ_ENOMEM;
        goto on_error;
    }
    codec->codec_data = ff;
    ff->pool = pool;
    ff->enc = desc->enc;
    ff->dec = desc->dec;
    ff->desc = desc;

    *p_codec = codec;
    return PJ_SUCCESS;

on_error:
    if (pool)
        pj_pool_release(pool);
    return status;
}

/*
 * Free codec.
 */
static pj_status_t ffmpeg_dealloc_codec( pjmedia_vid_codec_factory *factory, 
				         pjmedia_vid_codec *codec )
{
    ffmpeg_private *ff;
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ffmpeg_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    ff = (ffmpeg_private*) codec->codec_data;
    pool = ff->pool;
    codec->codec_data = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t ffmpeg_codec_init( pjmedia_vid_codec *codec, 
				      pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static void print_ffmpeg_err(int err)
{
#if LIBAVCODEC_VERSION_MAJOR >= 52 && LIBAVCODEC_VERSION_MINOR >= 72
    char errbuf[512];
    if (av_strerror(err, errbuf, sizeof(errbuf)) >= 0)
        PJ_LOG(1, (THIS_FILE, "ffmpeg err %d: %s", err, errbuf));
#else
    PJ_LOG(1, (THIS_FILE, "ffmpeg err %d", err));
#endif

}

static enum PixelFormat dec_get_format(struct AVCodecContext *s, 
                                          const enum PixelFormat * fmt)
{
    ffmpeg_private *ff = (ffmpeg_private*)s->opaque;
    enum PixelFormat def_fmt = *fmt;

    while (*fmt != -1) {
	if (*fmt == ff->expected_dec_fmt)
	    return *fmt;
	++fmt;
    }

    pj_assert(!"Inconsistency in supported formats");
    return def_fmt;
}


static pj_status_t open_ffmpeg_codec(ffmpeg_private *ff,
                                     pj_mutex_t *ff_mutex)
{
    enum PixelFormat pix_fmt;
    pj_status_t status;
    pjmedia_video_format_detail *vfd;

    status = pjmedia_format_id_to_PixelFormat(ff->param.dec_fmt.id,
                                              &pix_fmt);
    if (status != PJ_SUCCESS)
        return status;

    vfd = pjmedia_format_get_video_format_detail(&ff->param.enc_fmt, 
						 PJ_TRUE);
    ff->expected_dec_fmt = pix_fmt;

    while (((ff->param.dir & PJMEDIA_DIR_ENCODING) && ff->enc_ctx == NULL) ||
           ((ff->param.dir & PJMEDIA_DIR_DECODING) && ff->dec_ctx == NULL))
    {
        pjmedia_dir dir;
        AVCodecContext *ctx = NULL;
        AVCodec *codec = NULL;
        int err;

        /* Set which direction to open */
        if (ff->param.dir==PJMEDIA_DIR_ENCODING_DECODING && ff->enc!=ff->dec) {
            dir = ff->enc_ctx? PJMEDIA_DIR_DECODING : PJMEDIA_DIR_ENCODING;
        } else {
            dir = ff->param.dir;
        }

	/* Init ffmpeg codec context */
        ctx = avcodec_alloc_context();

        /* Common attributes */
        ctx->pix_fmt = pix_fmt;
        ctx->workaround_bugs = FF_BUG_AUTODETECT;
        ctx->opaque = ff;

        if (dir & PJMEDIA_DIR_ENCODING) {
            codec = ff->enc;

            /* Encoding only attributes */
	    ctx->width = vfd->size.w;
	    ctx->height = vfd->size.h;
            ctx->time_base.num = vfd->fps.denum;
            ctx->time_base.den = vfd->fps.num;
	    if (vfd->avg_bps) {
                ctx->bit_rate = vfd->avg_bps;
		if (vfd->max_bps > vfd->avg_bps)
		    ctx->bit_rate_tolerance = vfd->max_bps - vfd->avg_bps;
	    }

	    /* Libx264 experimental setting (it rejects ffmpeg defaults) */
	    if (ff->param.enc_fmt.id == PJMEDIA_FORMAT_H264) {
		ctx->me_range = 16;
		ctx->max_qdiff = 4;
		ctx->qmin = 10;
		ctx->qmax = 51;
		ctx->qcompress = 0.6f;
	    }

	    /* For encoder, should be better to be strict to the standards */
            ctx->strict_std_compliance = FF_COMPLIANCE_STRICT;
        }

        if (dir & PJMEDIA_DIR_DECODING) {
            codec = ff->dec;

            /* Decoding only attributes */

	    /* Width/height may be overriden by ffmpeg after first decoding. */
            ctx->width = ctx->coded_width = ff->param.dec_fmt.det.vid.size.w;
            ctx->height = ctx->coded_height = ff->param.dec_fmt.det.vid.size.h;

            /* For decoder, be more flexible */
            if (ff->param.dir!=PJMEDIA_DIR_ENCODING_DECODING || 
		ff->enc!=ff->dec)
	    {
                ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
	    }

            ctx->get_format = &dec_get_format;
        }

        /* avcodec_open() should be protected */
        pj_mutex_lock(ff_mutex);
        err = avcodec_open(ctx, codec);
        pj_mutex_unlock(ff_mutex);
        if (err < 0) {
            print_ffmpeg_err(err);
            return PJMEDIA_CODEC_EFAILED;
        }

        if (dir & PJMEDIA_DIR_ENCODING)
            ff->enc_ctx = ctx;
        if (dir & PJMEDIA_DIR_DECODING)
            ff->dec_ctx = ctx;
    }
    
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t ffmpeg_codec_open( pjmedia_vid_codec *codec, 
				      pjmedia_vid_codec_param *attr )
{
    ffmpeg_private *ff;
    pj_status_t status;
    pj_mutex_t *ff_mutex;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);
    ff = (ffmpeg_private*)codec->codec_data;

    pj_memcpy(&ff->param, attr, sizeof(*attr));

    /* Apply SDP fmtp attribute */
    if (ff->desc->parse_fmtp) {
	status = (*ff->desc->parse_fmtp)(ff);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Open the codec */
    ff_mutex = ((struct ffmpeg_factory*)codec->factory)->mutex;
    status = open_ffmpeg_codec(ff, ff_mutex);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Init format info and apply-param of decoder */
    ff->dec_vfi = pjmedia_get_video_format_info(NULL, ff->param.dec_fmt.id);
    if (!ff->dec_vfi) {
        status = PJ_EINVAL;
        goto on_error;
    }
    pj_bzero(&ff->dec_vafp, sizeof(ff->dec_vafp));
    ff->dec_vafp.size = ff->param.dec_fmt.det.vid.size;
    ff->dec_vafp.buffer = NULL;
    status = (*ff->dec_vfi->apply_fmt)(ff->dec_vfi, &ff->dec_vafp);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    /* Init format info and apply-param of encoder */
    ff->enc_vfi = pjmedia_get_video_format_info(NULL, ff->param.dec_fmt.id);
    if (!ff->enc_vfi) {
        status = PJ_EINVAL;
        goto on_error;
    }
    pj_bzero(&ff->enc_vafp, sizeof(ff->enc_vafp));
    ff->enc_vafp.size = ff->param.enc_fmt.det.vid.size;
    ff->enc_vafp.buffer = NULL;
    status = (*ff->enc_vfi->apply_fmt)(ff->enc_vfi, &ff->enc_vafp);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    /* Update codec attributes, e.g: encoding format may be changed by
     * SDP fmtp negotiation.
     */
    pj_memcpy(attr, &ff->param, sizeof(*attr));

    return PJ_SUCCESS;

on_error:
    ffmpeg_codec_close(codec);
    return status;
}

/*
 * Close codec.
 */
static pj_status_t ffmpeg_codec_close( pjmedia_vid_codec *codec )
{
    ffmpeg_private *ff;
    pj_mutex_t *ff_mutex;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);
    ff = (ffmpeg_private*)codec->codec_data;
    ff_mutex = ((struct ffmpeg_factory*)codec->factory)->mutex;

    pj_mutex_lock(ff_mutex);
    if (ff->enc_ctx) {
        avcodec_close(ff->enc_ctx);
        av_free(ff->enc_ctx);
    }
    if (ff->dec_ctx && ff->dec_ctx!=ff->enc_ctx) {
        avcodec_close(ff->dec_ctx);
        av_free(ff->dec_ctx);
    }
    ff->enc_ctx = NULL;
    ff->dec_ctx = NULL;
    pj_mutex_unlock(ff_mutex);

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  ffmpeg_codec_modify( pjmedia_vid_codec *codec, 
				         const pjmedia_vid_codec_param *attr)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    PJ_UNUSED_ARG(attr);
    PJ_UNUSED_ARG(ff);

    return PJ_ENOTSUP;
}

static pj_status_t  ffmpeg_codec_get_param(pjmedia_vid_codec *codec,
					   pjmedia_vid_codec_param *param)
{
    ffmpeg_private *ff;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    ff = (ffmpeg_private*)codec->codec_data;
    pj_memcpy(param, &ff->param, sizeof(*param));

    return PJ_SUCCESS;
}


static pj_status_t  ffmpeg_packetize ( pjmedia_vid_codec *codec,
                                       pj_uint8_t *buf,
                                       pj_size_t buf_len,
                                       unsigned *pos,
                                       const pj_uint8_t **payload,
                                       pj_size_t *payload_len)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    if (ff->desc->packetize) {
	return (*ff->desc->packetize)(buf, buf_len, pos,
                                      ff->param.enc_mtu, payload,
                                      payload_len);
    }

    return PJ_ENOTSUP;
}

static pj_status_t  ffmpeg_unpacketize(pjmedia_vid_codec *codec,
                                       const pj_uint8_t *payload,
                                       pj_size_t   payload_len,
                                       pj_uint8_t *buf,
                                       pj_size_t  *buf_len)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    if (ff->desc->unpacketize) {
        return (*ff->desc->unpacketize)(payload, payload_len,
                                        buf, buf_len);
    }
    
    return PJ_ENOTSUP;
}


/*
 * Encode frames.
 */
static pj_status_t ffmpeg_codec_encode( pjmedia_vid_codec *codec, 
				        const pjmedia_frame *input,
				        unsigned output_buf_len, 
				        pjmedia_frame *output)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    pj_uint8_t *p = (pj_uint8_t*)input->buf;
    AVFrame avframe;
    pj_uint8_t *out_buf = (pj_uint8_t*)output->buf;
    int out_buf_len = output_buf_len;
    int err;

    /* For some reasons (e.g: SSE/MMX usage), the avcodec_encode_video() must
     * have stack aligned to 16 bytes. Let's try to be safe by preparing the
     * 16-bytes aligned stack here, in case it's not managed by the ffmpeg.
     */
    PJ_ALIGN_DATA(pj_uint32_t i[4], 16);

    /* Check if encoder has been opened */
    PJ_ASSERT_RETURN(ff->enc_ctx, PJ_EINVALIDOP);

    avcodec_get_frame_defaults(&avframe);
    avframe.pts = input->timestamp.u64;
    
    for (i[0] = 0; i[0] < ff->enc_vfi->plane_cnt; ++i[0]) {
        avframe.data[i[0]] = p;
        avframe.linesize[i[0]] = ff->enc_vafp.strides[i[0]];
        p += ff->enc_vafp.plane_bytes[i[0]];
    }

    err = avcodec_encode_video(ff->enc_ctx, out_buf, out_buf_len, &avframe);
    if (err < 0) {
        print_ffmpeg_err(err);
        return PJMEDIA_CODEC_EFAILED;
    } else {
        output->size = err;
    }

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t ffmpeg_codec_decode( pjmedia_vid_codec *codec, 
				        const pjmedia_frame *input,
				        unsigned output_buf_len, 
				        pjmedia_frame *output)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    AVFrame avframe;
    AVPacket avpacket;
    int err, got_picture;

    /* Check if decoder has been opened */
    PJ_ASSERT_RETURN(ff->dec_ctx, PJ_EINVALIDOP);

    /* Reset output frame bit info */
    output->bit_info = 0;

    /* Validate output buffer size */
    if (ff->dec_vafp.framebytes > output_buf_len)
	return PJ_ETOOSMALL;

    /* Init frame to receive the decoded data, the ffmpeg codec context will
     * automatically provide the decoded buffer (single buffer used for the
     * whole decoding session, and seems to be freed when the codec context
     * closed).
     */
    avcodec_get_frame_defaults(&avframe);

    /* Init packet, the container of the encoded data */
    av_init_packet(&avpacket);
    avpacket.data = (pj_uint8_t*)input->buf;
    avpacket.size = input->size;

    /* ffmpeg warns:
     * - input buffer padding, at least FF_INPUT_BUFFER_PADDING_SIZE
     * - null terminated
     * Normally, encoded buffer is allocated more than needed, so lets just
     * bzero the input buffer end/pad, hope it will be just fine.
     */
    pj_bzero(avpacket.data+avpacket.size, FF_INPUT_BUFFER_PADDING_SIZE);

    output->bit_info = 0;
    output->timestamp = input->timestamp;

#if LIBAVCODEC_VERSION_MAJOR >= 52 && LIBAVCODEC_VERSION_MINOR >= 72
    avpacket.flags = AV_PKT_FLAG_KEY;
#else
    avpacket.flags = 0;
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 52 && LIBAVCODEC_VERSION_MINOR >= 72
    err = avcodec_decode_video2(ff->dec_ctx, &avframe, 
                                &got_picture, &avpacket);
#else
    err = avcodec_decode_video(ff->dec_ctx, &avframe,
                               &got_picture, avpacket.data, avpacket.size);
#endif
    if (err < 0) {
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	output->size = 0;
        print_ffmpeg_err(err);
        return PJMEDIA_CODEC_EFAILED;
    } else if (got_picture) {
        pjmedia_video_apply_fmt_param *vafp = &ff->dec_vafp;
        pj_uint8_t *q = (pj_uint8_t*)output->buf;
	unsigned i;

	/* Decoder output format is set by libavcodec, in case it is different
	 * to the configured param.
	 */
	if (ff->dec_ctx->pix_fmt != ff->expected_dec_fmt ||
	    ff->dec_ctx->coded_width != (int)vafp->size.w ||
	    ff->dec_ctx->coded_height != (int)vafp->size.h)
	{
	    pjmedia_format_id new_fmt_id;
	    pj_status_t status;

	    /* Get current raw format id from ffmpeg decoder context */
	    status = PixelFormat_to_pjmedia_format_id(ff->dec_ctx->pix_fmt, 
						      &new_fmt_id);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Update decoder format in param */
    	    ff->param.dec_fmt.id = new_fmt_id;
	    ff->param.dec_fmt.det.vid.size.w = ff->dec_ctx->coded_width;
	    ff->param.dec_fmt.det.vid.size.h = ff->dec_ctx->coded_height;

	    /* Re-init format info and apply-param of decoder */
	    ff->dec_vfi = pjmedia_get_video_format_info(NULL, ff->param.dec_fmt.id);
	    if (!ff->dec_vfi)
		return PJ_ENOTSUP;
	    pj_bzero(&ff->dec_vafp, sizeof(ff->dec_vafp));
	    ff->dec_vafp.size = ff->param.dec_fmt.det.vid.size;
	    ff->dec_vafp.buffer = NULL;
	    status = (*ff->dec_vfi->apply_fmt)(ff->dec_vfi, &ff->dec_vafp);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Notify application via the bit_info field of pjmedia_frame */
	    output->bit_info = PJMEDIA_VID_CODEC_EVENT_FMT_CHANGED;

	    /* Check provided buffer size after format changed */
	    if (vafp->framebytes > output_buf_len)
		return PJ_ETOOSMALL;
	}

	/* Get the decoded data */
	for (i = 0; i < ff->dec_vfi->plane_cnt; ++i) {
	    pj_uint8_t *p = avframe.data[i];

	    /* The decoded data may contain padding */
	    if (avframe.linesize[i]!=vafp->strides[i]) {
		/* Padding exists, copy line by line */
		pj_uint8_t *q_end;
                    
		q_end = q+vafp->plane_bytes[i];
		while(q < q_end) {
		    pj_memcpy(q, p, vafp->strides[i]);
		    q += vafp->strides[i];
		    p += avframe.linesize[i];
		}
	    } else {
		/* No padding, copy the whole plane */
		pj_memcpy(q, p, vafp->plane_bytes[i]);
		q += vafp->plane_bytes[i];
	    }
	}

	output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        output->size = vafp->framebytes;
    } else {
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	output->size = 0;
    }
    
    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t  ffmpeg_codec_recover( pjmedia_vid_codec *codec, 
				          unsigned output_buf_len, 
				          pjmedia_frame *output)
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(output_buf_len);
    PJ_UNUSED_ARG(output);

    return PJ_ENOTSUP;
}

#ifdef _MSC_VER
#   pragma comment( lib, "avcodec.lib")
#endif

#endif	/* PJMEDIA_HAS_FFMPEG_CODEC */


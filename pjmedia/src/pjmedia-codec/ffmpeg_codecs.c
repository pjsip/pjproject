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


typedef struct ffmpeg_codec_info {
    PJ_DECL_LIST_MEMBER(struct ffmpeg_codec_info);
    pjmedia_vid_codec_info       info;
    AVCodec                     *enc;
    AVCodec                     *dec;
} ffmpeg_codec_info;


/* FFMPEG codecs factory */
static struct ffmpeg_factory {
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr	*mgr;
    pj_pool_factory             *pf;
    pj_pool_t		        *pool;
    pj_mutex_t		        *mutex;
    ffmpeg_codec_info            codecs;
} ffmpeg_factory;


/* FFMPEG codecs private data. */
typedef struct ffmpeg_private {
    AVCodec                     *enc;
    AVCodec                     *dec;
    AVCodecContext              *enc_ctx;
    AVCodecContext              *dec_ctx;
    AVCodecParserContext        *dec_parser_ctx;

    /*
    pjmedia_frame               *pack_frms;
    unsigned                     pack_frm_cnt;
    unsigned                     pack_frm_max_cnt;
    */

    pjmedia_vid_codec_param      param;	    /**< Codec param.		    */
    pj_pool_t		        *pool;	    /**< Pool for each instance.    */
    pj_timestamp	         last_tx;   /**< Timestamp of last transmit.*/
    const pjmedia_video_format_info *vfi;
    pjmedia_video_apply_fmt_param    vafp;
} ffmpeg_private;


/*
 * Initialize and register FFMPEG codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_ffmpeg_init(pjmedia_vid_codec_mgr *mgr,
                                              pj_pool_factory *pf)
{
    pj_pool_t *pool;
    AVCodec *c;
    enum CodecID last_codec_id = CODEC_ID_NONE;
    pj_status_t status;

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
    pj_list_init(&ffmpeg_factory.codecs);

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

    /* Enum FFMPEG codecs */
    for (c=av_codec_next(NULL); c; c=av_codec_next(c)) 
    {
        ffmpeg_codec_info *ci;
        
        if (c->type != CODEC_TYPE_VIDEO)
            continue;

        /* Video encoder and decoder are usually implemented in separate
         * AVCodec instances.
         */

        if (c->id == last_codec_id) {
            /* This codec usually be the decoder, and not as much info as in
             * encoder can be fetched here.
             */
            pj_assert(!pj_list_empty(&ffmpeg_factory.codecs));
            ci = ffmpeg_factory.codecs.prev;
            pj_assert(ci->info.dir != PJMEDIA_DIR_ENCODING_DECODING);
            pj_assert(!ci->dec || !ci->enc);
        } else {
            pjmedia_format_id enc_fmt_id;
            pjmedia_format_id raw_fmt[PJMEDIA_VID_CODEC_MAX_DEC_FMT_CNT];
            unsigned raw_fmt_cnt = 0;
            unsigned raw_fmt_cnt_should_be = 0;

            /* Get encoded format id */
            status = CodecID_to_pjmedia_format_id(c->id, &enc_fmt_id);
            if (status != PJ_SUCCESS) {
                //PJ_LOG(5, (THIS_FILE, "Unrecognized ffmpeg codec id %d, "
                //                      "codec [%s/%s] ignored",
                //                      c->id, c->name, c->long_name));
                //enc_fmt_id = PJMEDIA_FORMAT_FFMPEG_UNKNOWN;

                /* Skip unrecognized encoding format ID */
                continue;
            }

            /* Get raw/decoded format ids */
            if (c->pix_fmts) {
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
            } else {
                /* Unknown raw format, ignore this codec? */
                continue;
            }

            if (raw_fmt_cnt < raw_fmt_cnt_should_be) {
                PJ_LOG(6, (THIS_FILE, "Codec [%s/%s] have %d raw formats, "
                                      "recognized only %d raw formats",
                                      c->name, c->long_name,
                                      raw_fmt_cnt_should_be, raw_fmt_cnt));
            }
            if (raw_fmt_cnt == 0) {
                PJ_LOG(5, (THIS_FILE, "No recognized raw format "
                                      "for codec [%s/%s], codec ignored",
                                      c->name, c->long_name));
                /* Comment this to see all ffmpeg codecs */
                continue;
            }

            ci = PJ_POOL_ZALLOC_T(pool, ffmpeg_codec_info);
            ci->info.fmt_id = enc_fmt_id;
            pj_cstr(&ci->info.encoding_name, c->name);
            ci->info.clock_rate = 90000;
            ci->info.dec_fmt_id_cnt = raw_fmt_cnt;
            pj_memcpy(ci->info.dec_fmt_id, raw_fmt, 
                      sizeof(raw_fmt[0])*raw_fmt_cnt);

            switch (enc_fmt_id) {
                case PJMEDIA_FORMAT_H263:
                    ci->info.pt = PJMEDIA_RTP_PT_H263;
                    break;
                case PJMEDIA_FORMAT_H261:
                    ci->info.pt = PJMEDIA_RTP_PT_H261;
                    break;
                default:
                    break;
            }

            if (c->supported_framerates) {
                const AVRational *fr = c->supported_framerates;
                while ((fr->num != 0 || fr->den != 0) && 
                       ci->info.fps_cnt < PJMEDIA_VID_CODEC_MAX_FPS_CNT)
                {
                    ci->info.fps[ci->info.fps_cnt].num = fr->num;
                    ci->info.fps[ci->info.fps_cnt].denum = fr->den;
                    ++ci->info.fps_cnt;
                    ++fr;
                }
            }

            pj_list_push_back(&ffmpeg_factory.codecs, ci);
        }

        if (c->encode) {
            ci->info.dir |= PJMEDIA_DIR_ENCODING;
            ci->enc = c;
        }
        if (c->decode) {
            ci->info.dir |= PJMEDIA_DIR_DECODING;
            ci->dec = c;
        }

        last_codec_id = c->id;
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


static ffmpeg_codec_info* find_codec(const pjmedia_vid_codec_info *info)
{
    ffmpeg_codec_info *ci = ffmpeg_factory.codecs.next;

    pj_mutex_lock(ffmpeg_factory.mutex);

    while (ci != &ffmpeg_factory.codecs) {
        if ((ci->info.fmt_id == info->fmt_id) &&
            ((ci->info.dir & info->dir) == info->dir) &&
            pj_stricmp(&ci->info.encoding_name, &info->encoding_name)==0)
        {
            pj_mutex_unlock(ffmpeg_factory.mutex);
            return ci;
        }
        ci = ci->next;
    }

    pj_mutex_unlock(ffmpeg_factory.mutex);

    return NULL;
}


/* 
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t ffmpeg_test_alloc( pjmedia_vid_codec_factory *factory, 
				      const pjmedia_vid_codec_info *info )
{
    ffmpeg_codec_info *ci;

    PJ_ASSERT_RETURN(factory==&ffmpeg_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info, PJ_EINVAL);

    ci = find_codec(info);
    if (!ci) {
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
    ffmpeg_codec_info *ci;

    PJ_ASSERT_RETURN(factory==&ffmpeg_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    ci = find_codec(info);
    if (!ci) {
        return PJMEDIA_CODEC_EUNSUP;
    }

    pj_bzero(attr, sizeof(pjmedia_vid_codec_param));
    attr->dir = ci->info.dir;
    attr->pt = info->pt;
    pjmedia_format_init_video(&attr->enc_fmt, ci->info.fmt_id,
                              352, 288, 25, 1);
    pjmedia_format_init_video(&attr->dec_fmt, ci->info.dec_fmt_id[0],
                              352, 288, 25, 1);

    return PJ_SUCCESS;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t ffmpeg_enum_codecs( pjmedia_vid_codec_factory *factory,
				       unsigned *count, 
				       pjmedia_vid_codec_info codecs[])
{
    ffmpeg_codec_info *ci;
    unsigned max;

    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ffmpeg_factory.base, PJ_EINVAL);

    max = *count;
    *count = 0;
    ci = ffmpeg_factory.codecs.next;

    pj_mutex_lock(ffmpeg_factory.mutex);
    while (*count < max && ci != &ffmpeg_factory.codecs) {
        pj_memcpy(&codecs[*count], &ci->info, sizeof(pjmedia_vid_codec_info));
        *count = *count + 1;
        ci = ci->next;
    }
    pj_mutex_unlock(ffmpeg_factory.mutex);

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
    ffmpeg_codec_info *ci;
    pjmedia_vid_codec *codec;
    pj_pool_t *pool = NULL;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(factory && info && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &ffmpeg_factory.base, PJ_EINVAL);

    ci = find_codec(info);
    if (!ci) {
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
    ff->enc = ci->enc;
    ff->dec = ci->dec;

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

/*
static enum PixelFormat ffdec_nego_format(struct AVCodecContext *s, 
                                          const enum PixelFormat * fmt)
{
    enum PixelFormat pix_fmt;

    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(fmt);

    pjmedia_format_id_to_PixelFormat(PJMEDIA_FORMAT_BGRA, &pix_fmt);
    return pix_fmt;
}

static void enc_got_payload(struct AVCodecContext *avctx,
                            void *data, int size, int mb_nb);
*/


static pj_status_t open_ffmpeg_codec(ffmpeg_private *ff,
                                     pj_mutex_t *ff_mutex)
{
    enum PixelFormat pix_fmt;
    pj_status_t status;

    status = pjmedia_format_id_to_PixelFormat(ff->param.dec_fmt.id,
                                              &pix_fmt);
    if (status != PJ_SUCCESS)
        return status;

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

        ctx = avcodec_alloc_context();

        /* Common attributes */
        ctx->pix_fmt = pix_fmt;
        ctx->width = ff->param.enc_fmt.det.vid.size.w;
        ctx->height = ff->param.enc_fmt.det.vid.size.h;
        ctx->workaround_bugs = FF_BUG_AUTODETECT;
        ctx->opaque = ff;

        if (dir & PJMEDIA_DIR_ENCODING) {
            codec = ff->enc;

            /* Encoding only attributes */
            ctx->time_base.num = ff->param.enc_fmt.det.vid.fps.denum;
            ctx->time_base.den = ff->param.enc_fmt.det.vid.fps.num;
            if (ff->param.enc_fmt.det.vid.avg_bps)
                ctx->bit_rate = ff->param.enc_fmt.det.vid.avg_bps;
            if (ff->param.enc_fmt.det.vid.max_bps)
                ctx->rc_max_rate = ff->param.enc_fmt.det.vid.max_bps;
#if 0
            if (ff->param.enc_mtu) {
                //ctx->rtp_payload_size = ff->param.enc_mtu;
                //ctx->rtp_callback = &enc_got_payload;

                /* Allocate frame array for RTP payload packing */
                if (ff->param.enc_fmt.det.vid.max_bps)
                    ff->pack_frm_max_cnt = ff->param.enc_fmt.det.vid.max_bps /
                                           ff->param.enc_mtu + 1;
                else
                    ff->pack_frm_max_cnt = 32;

                ff->pack_frms = (pjmedia_frame*)
                                pj_pool_calloc(ff->pool, ff->pack_frm_max_cnt,
                                               sizeof(ff->pack_frms[0]));
            }
#endif

            /* For encoder, should be better to be strict to the standards */
            ctx->strict_std_compliance = FF_COMPLIANCE_STRICT;
        }
        if (dir & PJMEDIA_DIR_DECODING) {
            codec = ff->dec;

            /* Decoding only attributes */
            ctx->coded_width = ctx->width;
            ctx->coded_height = ctx->height;

            /* For decoder, be more flexible */
            if (ff->param.dir!=PJMEDIA_DIR_ENCODING_DECODING || ff->enc!=ff->dec)
                ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

            //ctx->get_format = &ffdec_nego_format;
        }

        /* avcodec_open() should be protected */
        pj_mutex_lock(ff_mutex);
        err = avcodec_open(ctx, codec);
        pj_mutex_unlock(ff_mutex);
        if (err < 0) {
            print_ffmpeg_err(err);
            return PJ_EUNKNOWN;
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

    ff->vfi = pjmedia_get_video_format_info(NULL, attr->dec_fmt.id);
    if (!ff->vfi) {
        status = PJ_EINVAL;
        goto on_error;
    }

    pj_bzero(&ff->vafp, sizeof(ff->vafp));
    ff->vafp.size = attr->dec_fmt.det.vid.size;
    ff->vafp.buffer = 0;
    status = (*ff->vfi->apply_fmt)(ff->vfi, &ff->vafp);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    pj_memcpy(&ff->param, attr, sizeof(*attr));

    ff_mutex = ((struct ffmpeg_factory*)codec->factory)->mutex;
    status = open_ffmpeg_codec(ff, ff_mutex);
    if (status != PJ_SUCCESS)
        goto on_error;

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
    if (ff->dec_parser_ctx) {
        av_parser_close(ff->dec_parser_ctx);

    }
    ff->enc_ctx = NULL;
    ff->dec_ctx = NULL;
    ff->dec_parser_ctx = NULL;
    pj_mutex_unlock(ff_mutex);

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  ffmpeg_codec_modify( pjmedia_vid_codec *codec, 
				         const pjmedia_vid_codec_param *attr )
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    PJ_UNUSED_ARG(attr);
    PJ_UNUSED_ARG(ff);

    return PJ_ENOTSUP;
}

static pj_status_t  ffmpeg_packetize ( pjmedia_vid_codec *codec,
                                       pj_uint8_t *buf,
                                       pj_size_t buf_len,
                                       unsigned *pos,
                                       const pj_uint8_t **payload,
                                       pj_size_t *payload_len)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    switch (ff->param.enc_fmt.id) {
        case PJMEDIA_FORMAT_H263:
            return pjmedia_h263_packetize(buf, buf_len, pos,
                                          ff->param.enc_mtu, payload,
                                          payload_len);
            break;
        default:
            return PJ_ENOTSUP;
    }
}

static pj_status_t  ffmpeg_unpacketize(pjmedia_vid_codec *codec,
                                       const pj_uint8_t *payload,
                                       pj_size_t   payload_len,
                                       pj_uint8_t *buf,
                                       pj_size_t  *buf_len)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    switch (ff->param.enc_fmt.id) {
        case PJMEDIA_FORMAT_H263:
            return pjmedia_h263_unpacketize(payload, payload_len,
                                            buf, buf_len);
            break;
        default:
            return PJ_ENOTSUP;
    }
}

#if 0
/*
 * Pack encoded frame to RTP payload frames.
 */
static pj_status_t  ffmpeg_codec_pack ( pjmedia_vid_codec *codec,
				        const pjmedia_frame *enc_frame,
				        unsigned *frame_cnt,
				        pjmedia_frame frames[])
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    unsigned i;

    /* Can only work when encoding MTU is set */
    PJ_ASSERT_RETURN(ff->param.enc_mtu, PJ_EINVALIDOP);

    /* Validate available payload number */
    PJ_ASSERT_RETURN(ff->pack_frm_cnt <= *frame_cnt, PJ_EINVALIDOP);

    /* Validate encoded bitstream */
    PJ_ASSERT_RETURN(ff->pack_frm_cnt==0 || 
                     ff->pack_frms[0].buf == enc_frame->buf,
                     PJ_EINVAL);

    /* Return the payloads */
    *frame_cnt = ff->pack_frm_cnt;
    for (i = 0; i < *frame_cnt; ++i)
        frames[i] = ff->pack_frms[i];

    return PJ_SUCCESS;
}

/*
 * Get frames in the packet.
 */
static pj_status_t  ffmpeg_codec_parse( pjmedia_vid_codec *codec,
				        void *pkt,
				        pj_size_t pkt_size,
				        const pj_timestamp *ts,
				        pjmedia_frame *frame)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    pj_uint8_t *buf = frame->buf;
    int buf_size = frame->size;
    int processed;


    if (!ff->dec_parser_ctx) {
        ff->dec_parser_ctx = av_parser_init(ff->dec->id);
        if (!ff->dec_parser_ctx)
            return PJ_ENOTSUP;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 52 && LIBAVCODEC_VERSION_MINOR >= 72
    processed = av_parser_parse2(ff->dec_parser_ctx, ff->dec_ctx, 
                                 &buf, &buf_size, (uint8_t*)pkt, pkt_size,
                                 ts->u64, ts->u64, AV_NOPTS_VALUE);
#else
    processed = av_parser_parse (ff->dec_parser_ctx, ff->dec_ctx, 
                                 &buf, &buf_size, (uint8_t*)pkt, pkt_size,
                                 ts->u64, ts->u64);
#endif

    if (buf_size) {
        frame->timestamp = *ts;
        frame->size = buf_size;
        return PJ_SUCCESS;
    }

    return PJ_EPENDING;
}

static void enc_got_payload(struct AVCodecContext *avctx,
                            void *data, int size, int mb_nb)
{
    ffmpeg_private *ff = (ffmpeg_private*) avctx->opaque;
    pjmedia_frame *payload;

    pj_assert(ff->pack_frm_cnt < ff->pack_frm_max_cnt);
    payload = &ff->pack_frms[ff->pack_frm_cnt++];
    payload->buf = data;
    payload->size = size;
    payload->bit_info = mb_nb;
}

#endif


/*
 * Encode frames.
 */
static pj_status_t ffmpeg_codec_encode( pjmedia_vid_codec *codec, 
				        const struct pjmedia_frame *input,
				        unsigned output_buf_len, 
				        struct pjmedia_frame *output)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    pj_uint8_t *p = (pj_uint8_t*)input->buf;
    AVFrame avframe;
    pj_uint8_t *out_buf = (pj_uint8_t*)output->buf;
    int out_buf_len = output_buf_len;
    int err;
    unsigned i;

    /* Check if encoder has been opened */
    PJ_ASSERT_RETURN(ff->enc_ctx, PJ_EINVALIDOP);

    /*
    ff->pack_frm_cnt = 0;
    */
    avcodec_get_frame_defaults(&avframe);
    
    for (i = 0; i < ff->vfi->plane_cnt; ++i) {
        avframe.data[i] = p;
        avframe.linesize[i] = ff->vafp.strides[i];
        p += ff->vafp.plane_bytes[i];
    }

#ifdef _MSC_VER
    /* Align stack for MSVC environment to avoid 'random' crash, as advised in
     * http://ffmpeg.arrozcru.org/forum/viewtopic.php?f=1&t=549
     */
#   define VHALIGNCALL16(x) \
    {\
        _asm { mov ebx, esp }\
        _asm { and esp, 0xfffffff0 }\
        _asm { sub esp, 12 }\
        _asm { push ebx }\
        x;\
        _asm { pop ebx }\
        _asm { mov esp, ebx }\
     }
#else
#   define VHALIGNCALL16(x)
#endif

    VHALIGNCALL16(err = avcodec_encode_video(ff->enc_ctx, out_buf, 
                                             out_buf_len, &avframe));

    if (err < 0) {
        print_ffmpeg_err(err);
        return PJ_EUNKNOWN;
    } else {
        output->size = err;
    }

    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t ffmpeg_codec_decode( pjmedia_vid_codec *codec, 
				        const struct pjmedia_frame *input,
				        unsigned output_buf_len, 
				        struct pjmedia_frame *output)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;
    AVFrame avframe;
    AVPacket avpacket;
    int err, got_picture;

    /* Check if decoder has been opened */
    PJ_ASSERT_RETURN(ff->dec_ctx, PJ_EINVALIDOP);

    PJ_UNUSED_ARG(output_buf_len);

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
        print_ffmpeg_err(err);
        return PJ_EUNKNOWN;
    } else if (got_picture) {
        pjmedia_video_apply_fmt_param *vafp = (pjmedia_video_apply_fmt_param*)
                                              &ff->vafp;
        pj_uint8_t *q = (pj_uint8_t*)output->buf;
        unsigned i;

        /* Get the decoded data */
        for (i = 0; i < ff->vfi->plane_cnt; ++i) {
            pj_uint8_t *p = avframe.data[i];

            /* The decoded data may contain padding */
            if (avframe.linesize[i]==vafp->strides[i]) {
                /* No padding, copy the whole plane */
                pj_memcpy(q, p, vafp->plane_bytes[i]);
                q += vafp->plane_bytes[i];
            } else {
                /* Padding exists, copy line by line */
                pj_uint8_t *q_end;
                
                q_end = q+vafp->plane_bytes[i];
                while(q < q_end) {
                    pj_memcpy(q, p, vafp->strides[i]);
                    q += vafp->strides[i];
                    p += avframe.linesize[i];
                }
            }
        }
        output->size = vafp->framebytes;
    } else {
        return PJ_EUNKNOWN;
    }

    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t  ffmpeg_codec_recover( pjmedia_vid_codec *codec, 
				          unsigned output_buf_len, 
				          struct pjmedia_frame *output)
{
    ffmpeg_private *ff = (ffmpeg_private*)codec->codec_data;

    PJ_UNUSED_ARG(output_buf_len);
    PJ_UNUSED_ARG(output);
    PJ_UNUSED_ARG(ff);

    return PJ_SUCCESS;
}

#ifdef _MSC_VER
#   pragma comment( lib, "avcodec.lib")
#endif

#endif	/* PJMEDIA_HAS_FFMPEG_CODEC */


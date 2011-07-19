/* $Id$ */
/*
 * Copyright (C) 2010-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/types.h>
#include <pj/errno.h>

#if PJMEDIA_HAS_LIBAVFORMAT && PJMEDIA_HAS_LIBAVUTIL

#include "ffmpeg_util.h"
#include <libavformat/avformat.h>

/* Conversion table between pjmedia_format_id and PixelFormat */
static const struct ffmpeg_fmt_table_t
{
    pjmedia_format_id	id;
    enum PixelFormat	pf;
} ffmpeg_fmt_table[] =
{
    { PJMEDIA_FORMAT_RGBA, PIX_FMT_RGBA},
    { PJMEDIA_FORMAT_RGB24,PIX_FMT_RGB24},
    { PJMEDIA_FORMAT_BGRA, PIX_FMT_BGRA},

    { PJMEDIA_FORMAT_AYUV, PIX_FMT_NONE},
    { PJMEDIA_FORMAT_YUY2, PIX_FMT_YUYV422},
    { PJMEDIA_FORMAT_UYVY, PIX_FMT_UYVY422},
    { PJMEDIA_FORMAT_I420, PIX_FMT_YUV420P},
    { PJMEDIA_FORMAT_YV12, PIX_FMT_YUV422P},
    { PJMEDIA_FORMAT_I420JPEG, PIX_FMT_YUVJ420P},
    { PJMEDIA_FORMAT_I422JPEG, PIX_FMT_YUVJ422P},
};

/* Conversion table between pjmedia_format_id and CodecID */
static const struct ffmpeg_codec_table_t
{
    pjmedia_format_id	id;
    enum CodecID	codec_id;
} ffmpeg_codec_table[] =
{
    {PJMEDIA_FORMAT_H261,	CODEC_ID_H261},
    {PJMEDIA_FORMAT_H263,	CODEC_ID_H263},
    {PJMEDIA_FORMAT_H263P,	CODEC_ID_H263P},
    {PJMEDIA_FORMAT_H264,	CODEC_ID_H264},
    {PJMEDIA_FORMAT_MPEG1VIDEO,	CODEC_ID_MPEG1VIDEO},
    {PJMEDIA_FORMAT_MPEG2VIDEO, CODEC_ID_MPEG2VIDEO},
    {PJMEDIA_FORMAT_MPEG4,	CODEC_ID_MPEG4},
    {PJMEDIA_FORMAT_MJPEG,	CODEC_ID_MJPEG},
#if LIBAVCODEC_VERSION_MAJOR < 53
    {PJMEDIA_FORMAT_XVID,	CODEC_ID_XVID},
#endif
};

static int pjmedia_ffmpeg_ref_cnt;

void pjmedia_ffmpeg_add_ref()
{
    if (pjmedia_ffmpeg_ref_cnt++ == 0) {
	av_register_all();
    }
}

void pjmedia_ffmpeg_dec_ref()
{
    if (pjmedia_ffmpeg_ref_cnt-- == 1) {
	/* How to shutdown ffmpeg? */
    }

    if (pjmedia_ffmpeg_ref_cnt < 0) pjmedia_ffmpeg_ref_cnt = 0;
}

pj_status_t pjmedia_format_id_to_PixelFormat(pjmedia_format_id fmt_id,
					     enum PixelFormat *pixel_format)
{
    unsigned i;
    for (i=0; i<PJ_ARRAY_SIZE(ffmpeg_fmt_table); ++i) {
	const struct ffmpeg_fmt_table_t *t = &ffmpeg_fmt_table[i];
	if (t->id==fmt_id && t->pf != PIX_FMT_NONE) {
	    *pixel_format = t->pf;
	    return PJ_SUCCESS;
	}
    }

    *pixel_format = PIX_FMT_NONE;
    return PJ_ENOTFOUND;
}

pj_status_t PixelFormat_to_pjmedia_format_id(enum PixelFormat pf,
					     pjmedia_format_id *fmt_id)
{
    unsigned i;
    for (i=0; i<PJ_ARRAY_SIZE(ffmpeg_fmt_table); ++i) {
	const struct ffmpeg_fmt_table_t *t = &ffmpeg_fmt_table[i];
	if (t->pf == pf) {
	    if (fmt_id) *fmt_id = t->id;
	    return PJ_SUCCESS;
	}
    }

    return PJ_ENOTFOUND;
}

pj_status_t pjmedia_format_id_to_CodecID(pjmedia_format_id fmt_id,
					 enum CodecID *codec_id)
{
    unsigned i;
    for (i=0; i<PJ_ARRAY_SIZE(ffmpeg_codec_table); ++i) {
	const struct ffmpeg_codec_table_t *t = &ffmpeg_codec_table[i];
	if (t->id==fmt_id && t->codec_id != PIX_FMT_NONE) {
	    *codec_id = t->codec_id;
	    return PJ_SUCCESS;
	}
    }

    *codec_id = PIX_FMT_NONE;
    return PJ_ENOTFOUND;
}

pj_status_t CodecID_to_pjmedia_format_id(enum CodecID codec_id,
					 pjmedia_format_id *fmt_id)
{
    unsigned i;
    for (i=0; i<PJ_ARRAY_SIZE(ffmpeg_codec_table); ++i) {
	const struct ffmpeg_codec_table_t *t = &ffmpeg_codec_table[i];
	if (t->codec_id == codec_id) {
	    if (fmt_id) *fmt_id = t->id;
	    return PJ_SUCCESS;
	}
    }

    return PJ_ENOTFOUND;
}


#ifdef _MSC_VER
#   pragma comment( lib, "avformat.lib")
#   pragma comment( lib, "avutil.lib")
#endif

#endif	/* #if PJMEDIA_HAS_LIBAVFORMAT && PJMEDIA_HAS_LIBAVUTIL */

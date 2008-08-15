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
#ifndef __PJMEDIA_CODEC_CONFIG_H__
#define __PJMEDIA_CODEC_CONFIG_H__

#include <pjmedia/types.h>

/*
 * Include config_auto.h if autoconf is used (PJ_AUTOCONF is set)
 */
#if defined(PJ_AUTOCONF)
#   include <pjmedia-codec/config_auto.h>
#endif

/**
 * Unless specified otherwise, L16 codec is included by default.
 */
#ifndef PJMEDIA_HAS_L16_CODEC
#   define PJMEDIA_HAS_L16_CODEC    1
#endif


/**
 * Unless specified otherwise, GSM codec is included by default.
 */
#ifndef PJMEDIA_HAS_GSM_CODEC
#   define PJMEDIA_HAS_GSM_CODEC    1
#endif


/**
 * Unless specified otherwise, Speex codec is included by default.
 */
#ifndef PJMEDIA_HAS_SPEEX_CODEC
#   define PJMEDIA_HAS_SPEEX_CODEC    1
#endif

/**
 * Speex codec default complexity setting.
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY   2
#endif

/**
 * Speex codec default quality setting. Please note that pjsua-lib may override
 * this setting via its codec quality setting (i.e PJSUA_DEFAULT_CODEC_QUALITY).
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	    8
#endif


/**
 * Unless specified otherwise, iLBC codec is included by default.
 */
#ifndef PJMEDIA_HAS_ILBC_CODEC
#   define PJMEDIA_HAS_ILBC_CODEC    1
#endif


/**
 * Unless specified otherwise, G.722 codec is included by default.
 */
#ifndef PJMEDIA_HAS_G722_CODEC
#   define PJMEDIA_HAS_G722_CODEC    1
#endif


/**
 * IPP codecs are excluded by default. IPP codecs contain various codecs,
 * e.g: G.729, G.723.1, G.726, G.728, G.722.1, AMR.
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODECS
#   define PJMEDIA_HAS_INTEL_IPP_CODECS		0
#endif

/**
 * Specify IPP codecs content. If PJMEDIA_HAS_INTEL_IPP_CODECS is not set,
 * these settings will be ignored.
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMR	1
#endif

#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G729
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G729	1
#endif

#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G723
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G723	1
#endif

#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G726
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G726	1
#endif

#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G728
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G728	1
#endif

#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1	1
#endif


#endif	/* __PJMEDIA_CODEC_CONFIG_H__ */

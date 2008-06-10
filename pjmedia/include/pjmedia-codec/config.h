/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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


#endif	/* __PJMEDIA_CODEC_CONFIG_H__ */


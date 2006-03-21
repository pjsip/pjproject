/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_CONFIG_H__
#define __PJMEDIA_CONFIG_H__

#include <pj/config.h>

/**
 * Unless specified otherwise, PortAudio is enabled by default.
 */
#ifndef PJMEDIA_HAS_PORTAUDIO_SOUND
#   define PJMEDIA_HAS_PORTAUDIO_SOUND	    1
#endif


/**
 * Unless specified otherwise, Null sound is disabled.
 * This option is mutually exclusive with PortAudio sound, or otherwise
 * duplicate symbols error will occur.
 */
#ifndef PJMEDIA_HAS_NULL_SOUND
#   define PJMEDIA_HAS_NULL_SOUND	    0
#endif


/**
 * Unless specified otherwise, G711 codec is included by default.
 * Note that there are parts of G711 codec (such as linear2ulaw) that are 
 * needed by other PJMEDIA components (e.g. silence detector, conference).
 * Thus disabling G711 is generally not a good idea.
 */
#ifndef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC	    1
#endif


/**
 * Include small filter table in resample.
 * This adds about 9KB in rdata.
 */
#ifndef PJMEDIA_HAS_SMALL_FILTER
#   define PJMEDIA_HAS_SMALL_FILTER	    1
#endif


/**
 * Include large filter table in resample.
 * This adds about 32KB in rdata.
 */
#ifndef PJMEDIA_HAS_LARGE_FILTER
#   define PJMEDIA_HAS_LARGE_FILTER	    1
#endif


#endif	/* __PJMEDIA_CONFIG_H__ */


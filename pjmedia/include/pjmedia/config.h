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
 */
#ifndef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC	    1
#endif


#endif	/* __PJMEDIA_CONFIG_H__ */

/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_AUDIODEV_CONFIG_H__
#define __PJMEDIA_AUDIODEV_CONFIG_H__

/**
 * @file audiodev.h
 * @brief Audio device API.
 */
#include <pjmedia/port.h>
#include <pj/pool.h>


PJ_BEGIN_DECL

/**
 * @defgroup audio_device_api Audio Device API
 * @brief PJMEDIA audio device abstraction API.
 */

/**
 * @defgroup s1_audio_device_config Compile time configurations
 * @ingroup audio_device_api
 * @brief Compile time configurations
 * @{
 */

/**
 * This setting controls whether PortAudio support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
#   define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	0
#endif


/**
 * This setting controls whether WMME support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_WMME
#   define PJMEDIA_AUDIO_DEV_HAS_WMME		1
#endif


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_AUDIODEV_CONFIG_H__ */

/*
 --------------------- DOCUMENTATION FOLLOWS ---------------------------
 */

/**
 * @addtogroup audio_device_api Audio Device API
 * @{

PJMEDIA Audio Device API is a cross-platform audio API appropriate for use with
VoIP applications and many other types of audio streaming applications. 

The API abstracts many different audio API's on various platforms, such as:
 - PortAudio back-end for Win32, Windows Mobile, Linux, Unix, dan MacOS X.
 - native WMME audio for Win32 and Windows Mobile devices
 - native Symbian audio streaming/multimedia framework (MMF) implementation
 - native Nokia Audio Proxy Server (APS) implementation
 - null-audio implementation
 - and more to be implemented in the future

The Audio Device API/library is an evolution from PJMEDIA @ref PJMED_SND and 
contains many enhancements:

 - Forward compatibility:
\n
   The new API has been designed to be extensible, it will support new API's as 
   well as new features that may be introduced in the future without breaking 
   compatibility with applications that use this API as well as compatibility 
   with existing device implementations. 

 - Device capabilities:
\n
   At the heart of the API is device capabilities management, where all possible
   audio capabilities of audio devices should be able to be handled in a generic
   manner. With this framework, new capabilities that may be discovered in the 
   future can be handled in manner without breaking existing applications. 

 - Built-in features:
\n
   The device capabilities framework enables applications to use audio features
   built-in in the device, such as:
    - echo cancellation, 
    - built-in codecs, 
    - audio routing, and 
    - volume control. 

 - Codec support:
\n
   Some audio devices such as Nokia/Symbian Audio Proxy Server (APS) and Nokia 
   VoIP Audio Services (VAS) support built-in hardware audio codecs (e.g. G.729,
   iLBC, and AMR), and application can use the sound device in encoded mode to
   make use of these hardware codecs. 

 - Multiple backends:
\n
   The new API supports multiple audio backends (called factories or drivers in 
   the code) to be active simultaneously, and audio backends may be added or 
   removed during run-time. 
 */

/**
 * @}
 */


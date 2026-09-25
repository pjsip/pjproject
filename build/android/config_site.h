/* $Id$ */
/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*
 * Configuration for the binary (AAR) distribution of PJSIP on Android.
 * build-aar.sh installs this file as pjlib/include/pj/config_site.h.
 *
 * Most of the configuration is passed to CMake as options instead, and is
 * listed in build-aar.sh; only what CMake has no option for lives here.
 *
 * Unlike the Apple distribution this file is not part of a shipped ABI. The
 * AAR carries no headers: an application talks to the library through the
 * generated org.pjsip.pjsua2 Java classes, and the only native symbols it can
 * reach are the JNI entry points. So these values need to be right, but they
 * do not have to be frozen into anything a consumer compiles against.
 */
#ifndef __PJ_CONFIG_SITE_ANDROID_DIST_H__
#define __PJ_CONFIG_SITE_ANDROID_DIST_H__

/*
 * Deliberately not including config_site_sample.h. Its PJ_CONFIG_ANDROID
 * profile predates 64-bit Android and turns off floating point, which every
 * ABI this distribution targets has.
 */

/* Speex is built for its AEC and as a narrowband codec; 5 is the quality the
 * sample Android profile uses and keeps the bitrate sensible on mobile.
 */
#define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY 5
#define PJSUA_DEFAULT_CODEC_QUALITY         4

/*
 * No audio backend other than Oboe and the Java one is built. OpenSL ES is
 * superseded by Oboe, which uses AAudio where it can and falls back to
 * OpenSL ES itself.
 */
#define PJMEDIA_AUDIO_DEV_HAS_OPENSL        0
#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO     0
#define PJMEDIA_AUDIO_DEV_HAS_WMME          0

/*
 * DTLS-SRTP needs OpenSSL, which this distribution bundles, so unlike the
 * Apple build it is available and WebRTC interoperability works. Left at the
 * upstream default; named here only because that difference is easy to miss.
 */

#endif  /* __PJ_CONFIG_SITE_ANDROID_DIST_H__ */

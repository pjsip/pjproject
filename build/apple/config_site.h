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
 * Configuration for the binary (XCFramework) distribution of PJSIP on
 * Apple platforms. build-xcframework.sh installs this file as
 * pjlib/include/pj/config_site.h and ships a copy inside every slice of
 * the framework.
 *
 * Most of the configuration is passed to CMake as options instead, and is
 * listed in build-xcframework.sh; only what CMake has no option for lives
 * here. Either way the resulting values are frozen into the shipped headers,
 * because this file is part of the distributed ABI: public headers contain
 * inline code and structures whose layout depends on these macros, so a
 * consumer that compiles against different settings gets silent memory
 * corruption rather than a link error.
 */
#ifndef __PJ_CONFIG_SITE_APPLE_DIST_H__
#define __PJ_CONFIG_SITE_APPLE_DIST_H__

/*
 * DTLS-SRTP is implemented against OpenSSL only (transport_srtp_dtls.c), and
 * TLS here comes from Apple's Network framework, so it is unavailable.
 * SDES-SRTP is unaffected. CMake has no option for this.
 */
#undef  PJMEDIA_SRTP_HAS_DTLS
#define PJMEDIA_SRTP_HAS_DTLS               0

/* iLBC comes from CoreAudio rather than the bundled implementation. */
#define PJMEDIA_ILBC_CODEC_USE_COREAUDIO    1

/* Duck other audio based on voice activity rather than for the whole call
 * (requires iOS 17).
 */
#define PJMEDIA_AUDIO_DEV_COREAUDIO_ADVANCED_DUCKING 1

/* No audio backend other than CoreAudio is built. */
#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO     0
#define PJMEDIA_AUDIO_DEV_HAS_WMME          0

#define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY 5
#define PJSUA_DEFAULT_CODEC_QUALITY         4

/* build-xcframework.sh appends the frozen build macros below this line. */

#endif  /* __PJ_CONFIG_SITE_APPLE_DIST_H__ */

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
 * This file is part of the distributed ABI. Public headers contain inline
 * code and structures whose layout depends on these macros, so a consumer
 * that compiles against different settings gets silent memory corruption
 * rather than a link error. Everything not listed here is left at the
 * upstream default on purpose, so the contract is "upstream defaults plus
 * the deltas below".
 */
#ifndef __PJ_CONFIG_SITE_APPLE_DIST_H__
#define __PJ_CONFIG_SITE_APPLE_DIST_H__

/*
 * TLS: Apple's Network framework, never OpenSSL.
 *
 * The autoconf probe picks PJ_SSL_SOCK_IMP_DARWIN (deprecated Secure
 * Transport) on Darwin, so the choice is made here and --disable-darwin-ssl
 * is passed at configure time. os_auto.h is included before this file, hence
 * the #undef.
 */
#undef  PJ_HAS_SSL_SOCK
#define PJ_HAS_SSL_SOCK                     1
#undef  PJ_SSL_SOCK_IMP
#define PJ_SSL_SOCK_IMP                     PJ_SSL_SOCK_IMP_APPLE

/* The Apple backend drains its events from ssl_network_event_poll(), whose
 * only caller is the select() ioqueue. This is also the upstream default;
 * it is restated because the build fails at compile time without it.
 */
#undef  PJ_IOQUEUE_IMP
#define PJ_IOQUEUE_IMP                      PJ_IOQUEUE_IMP_SELECT

/* DTLS-SRTP is implemented against OpenSSL only (transport_srtp_dtls.c),
 * so it is unavailable in this build. SDES-SRTP is unaffected.
 */
#undef  PJMEDIA_SRTP_HAS_DTLS
#define PJMEDIA_SRTP_HAS_DTLS               0

/*
 * Codecs excluded from binary distribution for licensing reasons:
 * AMR-NB/WB carry patent claims, bcg729 is LGPL and static linking would
 * impose a relink obligation on every consumer. Also disabled at configure
 * time so the object code is never produced.
 */
#undef  PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
#define PJMEDIA_HAS_OPENCORE_AMRNB_CODEC    0
#undef  PJMEDIA_HAS_OPENCORE_AMRWB_CODEC
#define PJMEDIA_HAS_OPENCORE_AMRWB_CODEC    0
#undef  PJMEDIA_HAS_BCG729
#define PJMEDIA_HAS_BCG729                  0

/*
 * Video.
 *
 * PJMEDIA_HAS_VIDEO is never set by autoconf; it is a config_site decision.
 * Capture comes from AVFoundation and rendering from Metal and, on iOS,
 * OpenGL ES, all enabled by the darwin probes in aconfigure.
 *
 * H.264 is provided by VideoToolbox, which has no third party dependency and
 * is hardware backed. openh264, VPX and FFmpeg are excluded from this build,
 * so VideoToolbox is the only video codec.
 */
#define PJMEDIA_HAS_VIDEO                   1
#define PJMEDIA_HAS_VID_TOOLBOX_CODEC       1

/*
 * Apple platform settings.
 */
#define PJ_HAS_FLOATING_POINT               1

#define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO     0
#define PJMEDIA_AUDIO_DEV_HAS_WMME          0
#define PJMEDIA_AUDIO_DEV_HAS_COREAUDIO     1

/* CoreAudio has a built-in echo canceller. */
#define PJMEDIA_HAS_SPEEX_AEC               0

#define PJMEDIA_HAS_L16_CODEC               0

/* iLBC comes from CoreAudio. */
#define PJMEDIA_HAS_ILBC_CODEC              1
#define PJMEDIA_ILBC_CODEC_USE_COREAUDIO    1

#define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY 5
#define PJSUA_DEFAULT_CODEC_QUALITY         4

/* build-xcframework.sh appends the frozen build macros below this line. */

#endif  /* __PJ_CONFIG_SITE_APPLE_DIST_H__ */

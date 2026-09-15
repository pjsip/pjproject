/*
 * Tier 1: the shipped headers must describe exactly the binary that was built.
 *
 * Compiled against each slice's own Headers directory with no -D flags at all,
 * which also proves that PJ_AUTOCONF and the build's command line macros were
 * frozen into the headers. Every failure here is an ABI hazard: a consumer
 * compiling against different values than the library was built with gets
 * silent memory corruption, not a link error.
 */
#include <TargetConditionals.h>
#include <pj/config.h>
#include <pjmedia/config.h>
#include <pjmedia-codec/config.h>
#include <pjmedia-videodev/config.h>
#include <pjsip/sip_config.h>

#ifndef PJ_AUTOCONF
#  error "PJ_AUTOCONF was not frozen into the shipped headers"
#endif

/* Transport and security. */
#if PJ_HAS_SSL_SOCK != 1
#  error "PJ_HAS_SSL_SOCK must be 1"
#endif
#if PJ_SSL_SOCK_IMP != PJ_SSL_SOCK_IMP_APPLE
#  error "TLS backend must be the Apple Network framework"
#endif
#if PJ_IOQUEUE_IMP != PJ_IOQUEUE_IMP_SELECT
#  error "the Apple TLS backend only drains events from the select() ioqueue"
#endif
#if PJSIP_HAS_TLS_TRANSPORT != 1
#  error "SIP TLS transport must be enabled"
#endif
#if PJMEDIA_HAS_SRTP != 1
#  error "SRTP must be enabled"
#endif
#if PJMEDIA_SRTP_HAS_DTLS != 0
#  error "DTLS-SRTP must stay off: it is implemented against OpenSSL only"
#endif

/* Audio codecs that ship. */
#if PJMEDIA_HAS_G711_CODEC != 1 || PJMEDIA_HAS_G722_CODEC != 1
#  error "G.711 and G.722 must be present"
#endif
#if PJMEDIA_HAS_OPUS_CODEC != 1
#  error "Opus must be present"
#endif
#if PJMEDIA_HAS_ILBC_CODEC != 1
#  error "iLBC must be present"
#endif

/* Audio codecs excluded for licensing reasons. */
#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC != 0 || PJMEDIA_HAS_OPENCORE_AMRWB_CODEC != 0
#  error "AMR is patent encumbered and must not ship"
#endif
#if PJMEDIA_HAS_BCG729 != 0
#  error "bcg729 is LGPL and must not ship in a static archive"
#endif

/* Video. */
#if PJMEDIA_HAS_VIDEO != 1
#  error "video must be enabled"
#endif
#if PJMEDIA_HAS_VID_TOOLBOX_CODEC != 1
#  error "VideoToolbox H.264 must be enabled"
#endif
#if PJMEDIA_VIDEO_DEV_HAS_DARWIN != 1
#  error "AVFoundation capture must be enabled"
#endif
#if PJMEDIA_VIDEO_DEV_HAS_METAL != 1
#  error "the Metal renderer must be enabled"
#endif

/* Macros the build passes on the command line, frozen per slice. */
#if PJMEDIA_HAS_WEBRTC_AEC != 1
#  error "the WebRTC AEC was built in but the headers do not say so"
#endif
#if PJMEDIA_HAS_LIBYUV != 1
#  error "libyuv was built in but the headers do not say so"
#endif
#ifndef PJMEDIA_RESAMPLE_IMP
#  error "the resampler implementation was not frozen into the headers"
#endif

/* Renderers differ by platform, so the per-slice freeze must differ too. */
#if TARGET_OS_IPHONE
#  if PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL != 1
#    error "the iOS slice must carry the OpenGL ES renderer"
#  endif
#  if PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES != 1
#    error "PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES should follow from the iOS renderer"
#  endif
#else
#  if defined(PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL) && PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL != 0
#    error "the macOS slice must not claim the iOS OpenGL renderer"
#  endif
#endif

int main(void) { return 0; }

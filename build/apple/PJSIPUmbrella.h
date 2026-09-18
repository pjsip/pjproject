/*
 * Umbrella header for the PJSIP XCFramework.
 *
 * Not named PJSIP.h: the framework headers sit in one flat directory and
 * macOS filesystems are case insensitive, so that name would shadow
 * pjsip.h.
 *
 * Only the C API is exported to the Clang module. pjsua2 is C++ and cannot
 * be imported from Swift; its headers are still shipped in the framework
 * for use from C++ and Objective-C++ sources.
 */
#ifndef __PJSIP_UMBRELLA_H__
#define __PJSIP_UMBRELLA_H__

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjmedia_audiodev.h>
#include <pjmedia_videodev.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjsua.h>

#endif  /* __PJSIP_UMBRELLA_H__ */

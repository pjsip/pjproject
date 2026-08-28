/*
 * config_site.h -- PJSIP footprint measurement build.
 *
 * Profile: minimal embedded SIP endpoint (prospect target: STM32H563,
 * Cortex-M33, 640 KB RAM, 2 MB flash).
 *
 * Feature set: G.711 A/mu-law + G.722 + RFC 4733 telephone-event,
 * SDES-SRTP (bundled libsrtp), SIP over TLS via mbedTLS backend,
 * no video, no echo canceller backends, 4 concurrent calls (+1 margin).
 *
 * Used identically for both measurement builds:
 *   Build A: arm-linux-gnueabihf, -Os -mthumb, --gc-sections (flash/static RAM)
 *   Build B: x86 -m32 (peak heap at runtime)
 */

/* Start from the smallest-footprint preset. */
#define PJ_CONFIG_MINIMAL_SIZE
#include <pj/config_site_sample.h>

/*
 * Deviation from the preset (which sets log level 0): keep runtime
 * logging compiled in at level 4.  Needed to drive and verify the
 * runtime measurement, and closer to a shippable embedded config than
 * a fully silent build.  Flash cost of this choice is reported
 * separately in the report.
 */
#undef PJ_LOG_MAX_LEVEL
#define PJ_LOG_MAX_LEVEL                    4

/* Media features. */
#ifndef PJMEDIA_HAS_VIDEO
#   define PJMEDIA_HAS_VIDEO                0
#endif
#define PJMEDIA_HAS_SRTP                    1

/* Codecs: G.711 (PCMA/PCMU) + G.722 only.  RFC 4733 telephone-event
 * support is built into pjmedia_stream and needs no codec macro. */
#ifndef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC           1
#endif
#ifndef PJMEDIA_HAS_G722_CODEC
#   define PJMEDIA_HAS_G722_CODEC           1
#endif
#ifndef PJMEDIA_HAS_SPEEX_CODEC
#   define PJMEDIA_HAS_SPEEX_CODEC          0
#endif
#ifndef PJMEDIA_HAS_GSM_CODEC
#   define PJMEDIA_HAS_GSM_CODEC            0
#endif
#ifndef PJMEDIA_HAS_ILBC_CODEC
#   define PJMEDIA_HAS_ILBC_CODEC           0
#endif
#ifndef PJMEDIA_HAS_L16_CODEC
#   define PJMEDIA_HAS_L16_CODEC            0
#endif
#ifndef PJMEDIA_HAS_G7221_CODEC
#   define PJMEDIA_HAS_G7221_CODEC          0
#endif
#ifndef PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
#   define PJMEDIA_HAS_OPENCORE_AMRNB_CODEC 0
#endif
#ifndef PJMEDIA_HAS_OPENCORE_AMRWB_CODEC
#   define PJMEDIA_HAS_OPENCORE_AMRWB_CODEC 0
#endif
#ifndef PJMEDIA_HAS_OPUS_CODEC
#   define PJMEDIA_HAS_OPUS_CODEC           0
#endif
#ifndef PJMEDIA_HAS_SILK_CODEC
#   define PJMEDIA_HAS_SILK_CODEC           0
#endif

/* Application sizing: 4 concurrent calls + 1 margin, single account. */
#define PJSUA_MAX_CALLS                     5
#define PJSUA_MAX_ACC                       1

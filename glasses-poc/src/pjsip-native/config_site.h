/*
 * config_site.h for the Meta glasses PJSIP build.
 *
 * Copy to pjlib/include/pj/config_site.h before running
 * configure-android (build-android.sh does this). Tuned for: 16 kHz
 * open-ear audio, low-bitrate Opus, WebRTC AEC3, battery-conscious SIP.
 * Keep in sync with glasses-client/.../GlassesEpConfig.kt.
 */

#define PJ_CONFIG_ANDROID               1
#include <pj/config_site_sample.h>

/* ---- Audio pipeline ------------------------------------------------ */

/* Glasses mics/speakers are 16 kHz class devices; avoid 48 kHz resampling. */
#define PJSUA_DEFAULT_CLOCK_RATE        16000
#define PJSUA_DEFAULT_AUDIO_FRAME_PTIME 20
#define PJMEDIA_SND_DEFAULT_REC_LATENCY 60
#define PJMEDIA_SND_DEFAULT_PLAY_LATENCY 80

/* Open-ear speakers: short but loud echo path. AEC3 + NS + AGC. */
#define PJSUA_DEFAULT_EC_TAIL_LEN       200
#define PJMEDIA_HAS_WEBRTC_AEC3         1
#define PJMEDIA_HAS_WEBRTC_AEC          0
#define PJMEDIA_HAS_SPEEX_AEC           0

/* Opus: 16 kHz, 16 kbps VBR, FEC, complexity 4 (battery). */
#define PJMEDIA_HAS_OPUS_CODEC          1
#define PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE  16000
#define PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE     16000
#define PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY   4

/* Drop codecs we never negotiate to shrink the .so and codec enum time. */
#define PJMEDIA_HAS_G722_CODEC          0
#define PJMEDIA_HAS_SPEEX_CODEC         0
#define PJMEDIA_HAS_ILBC_CODEC          0
#define PJMEDIA_HAS_GSM_CODEC           0
#define PJMEDIA_HAS_G7221_CODEC         0
#define PJMEDIA_HAS_L16_CODEC           0

/* Software clock keeps RTP timing stable when the HAL sleeps (Android default). */
#define PJSUA_DEFAULT_SND_USE_SW_CLOCK  PJ_TRUE

/* ---- Video (VR Glasses passthrough only) --------------------------- */
#ifndef GLASSES_WITH_VIDEO
#   define PJMEDIA_HAS_VIDEO            0
#else
#   define PJMEDIA_HAS_VIDEO            1
#   define PJMEDIA_VIDEO_DEV_HAS_ANDROID 1
#   define PJMEDIA_HAS_ANDROID_MEDIACODEC 1
#endif

/* ---- SIP / NAT: fewer wakeups ------------------------------------- */

#define PJSUA_MAX_CALLS                 2
#define PJSUA_MAX_ACC                   2
#define PJSUA_REG_INTERVAL              600
#define PJSUA_REG_RETRY_INTERVAL        30
#define PJSIP_TCP_KEEP_ALIVE_INTERVAL   90
#define PJSIP_TLS_KEEP_ALIVE_INTERVAL   90
#define PJ_STUN_KEEP_ALIVE_SEC          60
#define PJ_ICE_SESS_KEEP_ALIVE_MIN      60
#define PJSIP_MAX_PKT_LEN               4000   /* INVITE with X-Meta-Vision-Context */
#define PJ_HAS_IPV6                     1

/* ---- Security ------------------------------------------------------ */
#define PJMEDIA_HAS_SRTP                1
#define PJSIP_HAS_TLS_TRANSPORT         1

/* ---- Footprint ----------------------------------------------------- */
#define PJ_LOG_MAX_LEVEL                4
#define PJMEDIA_HAS_RTCP_XR             0
#define PJSIP_HAS_DIGEST_MD5_AUTH       1

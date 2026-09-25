/* To suppress warning: label ‘TODO*’ defined but not used [-Wunused-label] */
#define PJ_TODO(x)

/* Exercise both Windows audio backends. WMME is enabled by default, WASAPI is
 * not on the desktop, so without this the desktop WASAPI backend would not
 * even be compiled by the tests. aud_dev_test then initializes whichever of
 * them finds devices at run time.
 */
#if defined(PJ_WIN32) && PJ_WIN32!=0 && defined(_MSC_VER) && \
    !(defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0) && \
    !(defined(PJ_WIN32_UWP) && PJ_WIN32_UWP!=0) && \
    !(defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8!=0)
    /* CMake passes these on the command line, so drop those first */
#   undef PJMEDIA_AUDIO_DEV_HAS_WMME
#   undef PJMEDIA_AUDIO_DEV_HAS_WASAPI
#   define PJMEDIA_AUDIO_DEV_HAS_WMME           1
#   define PJMEDIA_AUDIO_DEV_HAS_WASAPI         1
#endif

/* Temp workaround for MacOS rwmutex deadlock */
#if defined(PJ_DARWINOS) && PJ_DARWINOS
    #undef PJ_EMULATE_RWMUTEX
    #define PJ_EMULATE_RWMUTEX  1
#endif

#if PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL
#   define PJMEDIA_SRTP_HAS_DTLS 1
#endif
#define PJMEDIA_HAS_WEBRTC_AEC 1
#define PJMEDIA_CODEC_L16_HAS_8KHZ_MONO 1
#define PJMEDIA_CODEC_L16_HAS_8KHZ_STEREO 1
#define PJMEDIA_CODEC_L16_HAS_16KHZ_MONO 1
#define PJMEDIA_CODEC_L16_HAS_16KHZ_STEREO 1
#define PJMEDIA_CODEC_L16_HAS_48KHZ_MONO 1
#define PJMEDIA_CODEC_L16_HAS_48KHZ_STEREO 1
#define PJMEDIA_HAS_G7221_CODEC 1
#define PJMEDIA_HAS_G722_CODEC 1
#define PJSIP_HAS_SIPREC 1
#define PJ_EXCLUDE_BENCHMARK_TESTS 0

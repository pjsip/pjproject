


/*
 * This file (config_site_sample.h) contains various configuration
 * settings that I use for certain settings. 
 */


/*
 * Typical configuration for WinCE target.
 */
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#   if !defined(PJ_DEBUG) || PJ_DEBUG==0
#	define PJ_LOG_MAX_LEVEL		0
#	define PJ_ENABLE_EXTRA_CHECK	0
#   endif

#   define PJ_HAS_FLOATING_POINT	0

#   define PJMEDIA_HAS_G711_PLC		0
#   define PJMEDIA_HAS_SMALL_FILTER	0
#   define PJMEDIA_HAS_LARGE_FILTER	0
#   define PJMEDIA_HAS_L16_CODEC	0
/*#   define PJMEDIA_HAS_GSM_CODEC	0*/
#   define PJMEDIA_HAS_ILBC_CODEC	0
/*#   define PJMEDIA_HAS_SPEEX_CODEC	0*/
#   define PJMEDIA_HAS_SPEEX_AEC	0
#endif




//#define PJ_CONFIG_MINIMAL_SIZE
//#define PJ_CONFIG_MAXIMUM_SPEED


/*
 * This file (config_site_sample.h) contains various configuration
 * settings that I use for certain settings. 
 */


/*
 * Typical configuration for WinCE target.
 */
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#   define PJ_HAS_FLOATING_POINT	0

#   define PJMEDIA_HAS_G711_PLC		0
#   define PJMEDIA_HAS_SMALL_FILTER	1
#   define PJMEDIA_HAS_LARGE_FILTER	0
#   define PJMEDIA_HAS_L16_CODEC	0
/*#   define PJMEDIA_HAS_GSM_CODEC	0*/
/*#   define PJMEDIA_HAS_ILBC_CODEC	0*/
/*#   define PJMEDIA_HAS_SPEEX_CODEC	0*/
#   define PJMEDIA_HAS_SPEEX_AEC	0
#endif



/*
 * Minimum size
 */
#ifdef PJ_CONFIG_MINIMAL_SIZE

#   undef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK	0
#   define PJ_LOG_MAX_LEVEL		0
#   define PJ_ENABLE_EXTRA_CHECK	0
#   define PJ_HAS_ERROR_STRING		0
#   undef PJ_IOQUEUE_MAX_HANDLES
#   define PJ_IOQUEUE_MAX_HANDLES	16
#   define PJ_IOQUEUE_HAS_SAFE_UNREG	0
#   define PJSIP_MAX_TSX_COUNT		15
#   define PJSIP_MAX_DIALOG_COUNT	15
#   define PJSIP_UDP_SO_SNDBUF_SIZE	4000
#   define PJSIP_UDP_SO_RCVBUF_SIZE	4000
#   define PJSIP_AUTH_HEADER_CACHING	0
#   define PJSIP_AUTH_AUTO_SEND_NEXT	0
#   define PJSIP_AUTH_QOP_SUPPORT	0
#   define PJMEDIA_HAS_LARGE_FILTER	0
#   define PJMEDIA_HAS_SMALL_FILTER	0


#elif defined(PJ_CONFIG_MAXIMUM_SPEED)
#   define PJ_SCANNER_USE_BITWISE	0
#   undef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK	0
#   define PJ_LOG_MAX_LEVEL		3
#   define PJ_ENABLE_EXTRA_CHECK	0
#   define PJ_IOQUEUE_MAX_HANDLES	5000
#   define PJSIP_MAX_TSX_COUNT		((640*1024)-1)
#   define PJSIP_MAX_DIALOG_COUNT	((640*1024)-1)
#   define PJSIP_UDP_SO_SNDBUF_SIZE	(24*1024*1024)
#   define PJSIP_UDP_SO_RCVBUF_SIZE	(24*1024*1024)
#   define PJ_DEBUG			0
#   define PJSIP_SAFE_MODULE		0
#   define PJ_HAS_STRICMP_ALNUM		0
#   define PJ_HASH_USE_OWN_TOLOWER	1
#   define PJSIP_UNESCAPE_IN_PLACE	1

#   ifdef PJ_WIN32
#     define PJSIP_MAX_NET_EVENTS	10
#   endif

#   define PJSUA_MAX_CALLS		512
#   define PJSIP_AUTH_HEADER_CACHING	1
#   define PJSIP_AUTH_AUTO_SEND_NEXT	1

#endif


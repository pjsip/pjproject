

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
//#   define PJMEDIA_HAS_SMALL_FILTER	1
//#   define PJMEDIA_HAS_LARGE_FILTER	0
#   define PJMEDIA_HAS_L16_CODEC	0
/*#   define PJMEDIA_HAS_GSM_CODEC	0*/
/*#   define PJMEDIA_HAS_ILBC_CODEC	0*/
/*#   define PJMEDIA_HAS_SPEEX_CODEC	0*/
#   define PJMEDIA_HAS_SPEEX_AEC	0
#   undef PJMEDIA_RESAMPLE_IMP
#   define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_LIBRESAMPLE
#endif


/*
 * Typical configuration for Symbian OS target
 */
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0

    /* We don't want to use float, for now */
#   undef PJ_HAS_FLOATING_POINT
#   define PJ_HAS_FLOATING_POINT	0

#   define PJMEDIA_SOUND_IMPLEMENTATION PJMEDIA_SOUND_NULL_SOUND

    /* Disable these */
#   define PJMEDIA_RESAMPLE_IMP		PJMEDIA_RESAMPLE_NONE
#   define PJMEDIA_HAS_SPEEX_AEC	0

    /* Disable all codecs but G.711, for now */
#   define PJMEDIA_HAS_L16_CODEC	0
#   define PJMEDIA_HAS_GSM_CODEC	0
#   define PJMEDIA_HAS_ILBC_CODEC	0
#   define PJMEDIA_HAS_SPEEX_CODEC	0

    /* Disable safe module access */
#   define PJSIP_SAFE_MODULE		0

#   define PJSIP_MAX_PKT_LEN		2000

    /* Since we don't have threads, log buffer can use static buffer */
#   define PJ_LOG_USE_STACK_BUFFER	0

    /* Symbian has problem with too many large blocks */
#   define PJSIP_POOL_LEN_ENDPT		1000
#   define PJSIP_POOL_INC_ENDPT		1000
#   define PJSIP_POOL_RDATA_LEN		2000
#   define PJSIP_POOL_RDATA_INC		2000
#   define PJSIP_POOL_LEN_TDATA		2000
#   define PJSIP_POOL_INC_TDATA		512
#   define PJSIP_POOL_LEN_UA		2000
#   define PJSIP_POOL_INC_UA		1000
#   define PJSIP_POOL_TSX_LAYER_LEN	256
#   define PJSIP_POOL_TSX_LAYER_INC	256
#   define PJSIP_POOL_TSX_LEN		512
#   define PJSIP_POOL_TSX_INC		128

    /* Set maximum number of dialog/transaction/calls to minimum */
#   define PJSIP_MAX_TSX_COUNT 		31
#   define PJSIP_MAX_DIALOG_COUNT 	31
#   define PJSUA_MAX_CALLS		31
    
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

#endif


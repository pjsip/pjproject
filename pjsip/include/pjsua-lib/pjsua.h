/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJSUA_H__
#define __PJSUA_H__

/**
 * @file pjsua.h
 * @brief PJSUA API.
 */


/* Include all PJSIP core headers. */
#include <pjsip.h>

/* Include all PJMEDIA headers. */
#include <pjmedia.h>

/* Include all PJMEDIA-CODEC headers. */
#include <pjmedia-codec.h>

/* Videodev too */
#include <pjmedia_videodev.h>

/* Include all PJSIP-UA headers */
#include <pjsip_ua.h>

/* Include all PJSIP-SIMPLE headers */
#include <pjsip_simple.h>

/* Include all PJNATH headers */
#include <pjnath.h>

/* Include all PJLIB-UTIL headers. */
#include <pjlib-util.h>

/* Include all PJLIB headers. */
#include <pjlib.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJSUA_LIB PJSUA API - High Level Softphone API
 * @brief Very high level API for constructing SIP UA applications.
 * @{
 *
 * @section pjsua_api_intro A SIP User Agent API for C/C++
 *
 * PJSUA API is very high level API for constructing SIP multimedia user agent
 * applications. It wraps together the signaling and media functionalities
 * into an easy to use call API, provides account management, buddy
 * management, presence, instant messaging, along with multimedia
 * features such as conferencing, file streaming, local playback,
 * voice recording, and so on.
 *
 * @subsection pjsua_for_c_cpp C/C++ Binding
 * Application must link with <b>pjsua-lib</b> to use this API. In addition,
 * this library depends on the following libraries:
 *  - <b>pjsip-ua</b>, 
 *  - <b>pjsip-simple</b>, 
 *  - <b>pjsip-core</b>, 
 *  - <b>pjmedia</b>,
 *  - <b>pjmedia-codec</b>, 
 *  - <b>pjlib-util</b>, and
 *  - <b>pjlib</b>, 
 *
 * so application must also link with these libraries as well. For more 
 * information, please refer to 
 * <A HREF="http://www.pjsip.org/using.htm">Getting Started with PJSIP</A>
 * page.
 *
 * @section pjsua_samples
 *
 * Few samples are provided:
 *
  - @ref page_pjsip_sample_simple_pjsuaua_c \n
    Very simple SIP User Agent with registration, call, and media, using
    PJSUA-API, all in under 200 lines of code.

  - @ref page_pjsip_samples_pjsua \n
    This is the reference implementation for PJSIP and PJMEDIA.
    PJSUA is a console based application, designed to be simple enough
    to be readble, but powerful enough to demonstrate all features
    available in PJSIP and PJMEDIA.\n

 * @section root_using_pjsua_lib Using PJSUA API
 *
 * Please refer to @ref PJSUA_LIB_BASE on how to create and initialize the API.
 * And then see the Modules on the bottom of this page for more information
 * about specific subject.
 */ 



/*****************************************************************************
 * BASE API
 */

/**
 * @defgroup PJSUA_LIB_BASE PJSUA-API Basic API
 * @ingroup PJSUA_LIB
 * @brief Basic application creation/initialization, logging configuration, etc.
 * @{
 *
 * The base PJSUA API controls PJSUA creation, initialization, and startup, and
 * also provides various auxiliary functions.
 *
 * @section using_pjsua_lib Using PJSUA Library
 *
 * @subsection creating_pjsua_lib Creating PJSUA
 *
 * Before anything else, application must create PJSUA by calling 
 * #pjsua_create().
 * This, among other things, will initialize PJLIB, which is crucial before 
 * any PJLIB functions can be called, PJLIB-UTIL, and create a SIP endpoint.
 *
 * After this function is called, application can create a memory pool (with
 * #pjsua_pool_create()) and read configurations from command line or file to
 * build the settings to initialize PJSUA below.
 *
 * @subsection init_pjsua_lib Initializing PJSUA
 *
 * After PJSUA is created, application can initialize PJSUA by calling
 * #pjsua_init(). This function takes several optional configuration settings 
 * in the argument, if application wants to set them.
 *
 * @subsubsection init_pjsua_lib_c_cpp PJSUA-LIB Initialization (in C)
 * Sample code to initialize PJSUA in C code:
 \code

 #include <pjsua-lib/pjsua.h>

 #define THIS_FILE  __FILE__

 static pj_status_t app_init(void)
 {
    pjsua_config	 ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config   media_cfg;
    pj_status_t status;

    // Must create pjsua before anything else!
    status = pjsua_create();
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing pjsua", status);
	return status;
    }

    // Initialize configs with default settings.
    pjsua_config_default(&ua_cfg);
    pjsua_logging_config_default(&log_cfg);
    pjsua_media_config_default(&media_cfg);

    // At the very least, application would want to override
    // the call callbacks in pjsua_config:
    ua_cfg.cb.on_incoming_call = ...
    ua_cfg.cb.on_call_state = ..
    ...

    // Customize other settings (or initialize them from application specific
    // configuration file):
    ...

    // Initialize pjsua
    status = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) {
          pjsua_perror(THIS_FILE, "Error initializing pjsua", status);
	  return status;
    }
    .
    ...
 }
 \endcode
 *
 *


 * @subsection other_init_pjsua_lib Other Initialization
 *
 * After PJSUA is initialized with #pjsua_init(), application will normally
 * need/want to perform the following tasks:
 *
 *  - create SIP transport with #pjsua_transport_create(). Application would
 *    to call #pjsua_transport_create() for each transport types that it
 *    wants to support (for example, UDP, TCP, and TLS). Please see
 *    @ref PJSUA_LIB_TRANSPORT section for more info.
 *  - create one or more SIP accounts with #pjsua_acc_add() or
 *    #pjsua_acc_add_local(). The SIP account is used for registering with
 *    the SIP server, if any. Please see @ref PJSUA_LIB_ACC for more info.
 *  - add one or more buddies with #pjsua_buddy_add(). Please see
 *    @ref PJSUA_LIB_BUDDY section for more info.
 *  - optionally configure the sound device, codec settings, and other
 *    media settings. Please see @ref PJSUA_LIB_MEDIA for more info.
 *
 *
 * @subsection starting_pjsua_lib Starting PJSUA
 *
 * After all initializations have been done, application must call
 * #pjsua_start() to start PJSUA. This function will check that all settings
 * have been properly configured, and apply default settings when they haven't,
 * or report error status when it is unable to recover from missing settings.
 *
 * Most settings can be changed during run-time. For example, application
 * may add, modify, or delete accounts, buddies, or change media settings
 * during run-time.
 *
 * @subsubsection starting_pjsua_lib_c C Example for Starting PJSUA
 * Sample code:
 \code
 static pj_status_t app_run(void)
 {
    pj_status_t status;

    // Start pjsua
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
	pjsua_destroy();
	pjsua_perror(THIS_FILE, "Error starting pjsua", status);
	return status;
    }

    // Run application loop
    while (1) {
	char choice[10];
	
	printf("Select menu: ");
	fgets(choice, sizeof(choice), stdin);
	...
    }
 }
 \endcode

 */

/** Constant to identify invalid ID for all sorts of IDs. */
enum pjsua_invalid_id_const_
{
    PJSUA_INVALID_ID = -1
};

/** Disabled features temporarily for media reorganization */
#define DISABLED_FOR_TICKET_1185	0

/** Call identification */
typedef int pjsua_call_id;

/** Account identification */
typedef int pjsua_acc_id;

/** Buddy identification */
typedef int pjsua_buddy_id;

/** File player identification */
typedef int pjsua_player_id;

/** File recorder identification */
typedef int pjsua_recorder_id;

/** Conference port identification */
typedef int pjsua_conf_port_id;

/** Opaque declaration for server side presence subscription */
typedef struct pjsua_srv_pres pjsua_srv_pres;

/** Forward declaration for pjsua_msg_data */
typedef struct pjsua_msg_data pjsua_msg_data;

/** Forward declaration for pj_stun_resolve_result */
typedef struct pj_stun_resolve_result pj_stun_resolve_result;

/**
 * Initial memory block for PJSUA.
 */
#ifndef PJSUA_POOL_LEN
#   define PJSUA_POOL_LEN		1000
#endif

/**
 * Memory increment for PJSUA.
 */
#ifndef PJSUA_POOL_INC
#   define PJSUA_POOL_INC		1000
#endif

/**
 * Initial memory block for PJSUA account.
 */
#ifndef PJSUA_POOL_LEN_ACC
#   define PJSUA_POOL_LEN_ACC	512
#endif

/**
 * Memory increment for PJSUA account.
 */
#ifndef PJSUA_POOL_INC_ACC
#   define PJSUA_POOL_INC_ACC	256
#endif

/**
 * Maximum proxies in account.
 */
#ifndef PJSUA_ACC_MAX_PROXIES
#   define PJSUA_ACC_MAX_PROXIES    8
#endif

/**
 * Default value of SRTP mode usage. Valid values are PJMEDIA_SRTP_DISABLED, 
 * PJMEDIA_SRTP_OPTIONAL, and PJMEDIA_SRTP_MANDATORY.
 */
#ifndef PJSUA_DEFAULT_USE_SRTP
    #define PJSUA_DEFAULT_USE_SRTP  PJMEDIA_SRTP_DISABLED
#endif

/**
 * Default value of secure signaling requirement for SRTP.
 * Valid values are:
 *	0: SRTP does not require secure signaling
 *	1: SRTP requires secure transport such as TLS
 *	2: SRTP requires secure end-to-end transport (SIPS)
 */
#ifndef PJSUA_DEFAULT_SRTP_SECURE_SIGNALING
    #define PJSUA_DEFAULT_SRTP_SECURE_SIGNALING 1
#endif

/**
 * Controls whether PJSUA-LIB should add ICE media feature tag
 * parameter (the ";+sip.ice" parameter) to Contact header if ICE
 * is enabled in the config.
 *
 * Default: 1
 */
#ifndef PJSUA_ADD_ICE_TAGS
#   define PJSUA_ADD_ICE_TAGS		1
#endif

/**
 * Timeout value used to acquire mutex lock on a particular call.
 *
 * Default: 2000 ms
 */
#ifndef PJSUA_ACQUIRE_CALL_TIMEOUT
#   define PJSUA_ACQUIRE_CALL_TIMEOUT 2000
#endif

/**
 * Is video enabled.
 */
#ifndef PJSUA_HAS_VIDEO
#   define PJSUA_HAS_VIDEO		PJMEDIA_HAS_VIDEO
#endif


/**
 * Interval between two keyframe requests, in milliseconds.
 *
 * Default: 3000 ms
 */
#ifndef PJSUA_VID_REQ_KEYFRAME_INTERVAL
#   define PJSUA_VID_REQ_KEYFRAME_INTERVAL	3000
#endif


/**
 * Specify whether timer heap events will be polled by a separate worker
 * thread. If this is set/enabled, a worker thread will be dedicated to
 * poll timer heap events only, and the rest worker thread(s) will poll
 * ioqueue/network events only.
 *
 * Note that if worker thread count setting (i.e: pjsua_config.thread_cnt)
 * is set to zero, this setting will be ignored.
 *
 * Default: 0 (disabled)
 */
#ifndef PJSUA_SEPARATE_WORKER_FOR_TIMER
#   define PJSUA_SEPARATE_WORKER_FOR_TIMER	0
#endif


/**
 * Specify whether pjsua should disable automatically sending initial
 * answer 100/Trying for incoming calls. If disabled, application can
 * later send 100/Trying if it wishes using pjsua_call_answer().
 *
 * Default: 0 (automatic sending enabled)
 */
#ifndef PJSUA_DISABLE_AUTO_SEND_100
#   define PJSUA_DISABLE_AUTO_SEND_100	0
#endif


/**
 * Default options that will be passed when creating ice transport.
 * See #pjmedia_transport_ice_options.
 */
#ifndef PJSUA_ICE_TRANSPORT_OPTION
#   define PJSUA_ICE_TRANSPORT_OPTION	0
#endif

/**
 * Interval of checking for any new ICE candidate when trickle ICE is active.
 * Trickle ICE gathers local ICE candidates, such as STUN and TURN candidates,
 * in the background, while SDP offer/answer negotiation is being performed.
 * Later, when any new ICE candidate is found, the endpoint will convey
 * the candidate to the remote endpoint via SIP INFO.
 *
 * Default: 100 ms
 */
#ifndef PJSUA_TRICKLE_ICE_NEW_CAND_CHECK_INTERVAL
#   define PJSUA_TRICKLE_ICE_NEW_CAND_CHECK_INTERVAL	100
#endif


/**
 * This enumeration represents pjsua state.
 */
typedef enum pjsua_state
{
    /**
     * The library has not been initialized.
     */
    PJSUA_STATE_NULL,

    /**
     * After pjsua_create() is called but before pjsua_init() is called.
     */
    PJSUA_STATE_CREATED,

    /**
     * After pjsua_init() is called but before pjsua_start() is called.
     */
    PJSUA_STATE_INIT,

    /**
     * After pjsua_start() is called but before everything is running.
     */
    PJSUA_STATE_STARTING,

    /**
     * After pjsua_start() is called and before pjsua_destroy() is called.
     */
    PJSUA_STATE_RUNNING,

    /**
     * After pjsua_destroy() is called but before the function returns.
     */
    PJSUA_STATE_CLOSING

} pjsua_state;


/**
 * Logging configuration, which can be (optionally) specified when calling
 * #pjsua_init(). Application must call #pjsua_logging_config_default() to
 * initialize this structure with the default values.
 */
typedef struct pjsua_logging_config
{
    /**
     * Log incoming and outgoing SIP message? Yes!
     */
    pj_bool_t	msg_logging;

    /**
     * Input verbosity level. Value 5 is reasonable.
     */
    unsigned	level;

    /**
     * Verbosity level for console. Value 4 is reasonable.
     */
    unsigned	console_level;

    /**
     * Log decoration.
     */
    unsigned	decor;

    /**
     * Optional log filename.
     */
    pj_str_t	log_filename;

    /**
     * Additional flags to be given to #pj_file_open() when opening
     * the log file. By default, the flag is PJ_O_WRONLY. Application
     * may set PJ_O_APPEND here so that logs are appended to existing
     * file instead of overwriting it.
     *
     * Default is 0.
     */
    unsigned	log_file_flags;

    /**
     * Optional callback function to be called to write log to
     * application specific device. This function will be called for
     * log messages on input verbosity level.
     */
    void       (*cb)(int level, const char *data, int len);


} pjsua_logging_config;


/**
 * Use this function to initialize logging config.
 *
 * @param cfg	The logging config to be initialized.
 */
PJ_DECL(void) pjsua_logging_config_default(pjsua_logging_config *cfg);


/**
 * Use this function to duplicate logging config.
 *
 * @param pool	    Pool to use.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_logging_config_dup(pj_pool_t *pool,
				       pjsua_logging_config *dst,
				       const pjsua_logging_config *src);


/**
 * Structure to be passed on MWI callback.
 */
typedef struct pjsua_mwi_info
{
    pjsip_evsub	    *evsub;	/**< Event subscription session, for
				     reference.				*/
    pjsip_rx_data   *rdata;	/**< The received NOTIFY request.	*/
} pjsua_mwi_info;


/**
 * Structure to be passed on registration callback.
 */
typedef struct pjsua_reg_info
{
    struct pjsip_regc_cbparam	*cbparam;   /**< Parameters returned by
						 registration callback.	*/
    pjsip_regc			*regc;	    /**< Client registration 
						 structure. */	
    pj_bool_t			 renew;     /**< Non-zero for registration and 
						 zero for unregistration. */
} pjsua_reg_info;


/**
 * Media stream info.
 */
typedef struct pjsua_stream_info
{
    /** Media type of this stream. */
    pjmedia_type type;

    /** Stream info (union). */
    union {
	/** Audio stream info */
	pjmedia_stream_info	aud;

	/** Video stream info */
	pjmedia_vid_stream_info	vid;
    } info;

} pjsua_stream_info;


/**
 * Media stream statistic.
 */
typedef struct pjsua_stream_stat
{
    /** RTCP statistic. */
    pjmedia_rtcp_stat	rtcp;

    /** Jitter buffer statistic. */
    pjmedia_jb_state	jbuf;

} pjsua_stream_stat;


/**
 * Structure to be passed to on stream precreate callback.
 * See on_stream_precreate().
 */
typedef struct pjsua_on_stream_precreate_param
{
    /**
     * Stream index in the media session, read-only.
     */
    unsigned            stream_idx;

    /**
     * Parameters that the stream will be created from.
     */
    pjsua_stream_info   stream_info;
} pjsua_on_stream_precreate_param;


/**
 * Structure to be passed to on stream created callback.
 * See on_stream_created2().
 */
typedef struct pjsua_on_stream_created_param
{
    /**
     * The audio media stream, read-only.
     */
    pjmedia_stream 	*stream;

    /**
     * Stream index in the audio media session, read-only.
     */
    unsigned 		 stream_idx;

    /**
     * Specify if PJSUA should take ownership of the port returned in
     * the port parameter below. If set to PJ_TRUE,
     * pjmedia_port_destroy() will be called on the port when it is
     * no longer needed.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t 		 destroy_port;

    /**
     * On input, it specifies the audio media port of the stream. Application
     * may modify this pointer to point to different media port to be
     * registered to the conference bridge.
     */
    pjmedia_port        *port;

} pjsua_on_stream_created_param;


/** 
 * Enumeration of media transport state types.
 */
typedef enum pjsua_med_tp_st
{
    /** Null, this is the state before media transport is created. */
    PJSUA_MED_TP_NULL,

    /**
     * Just before media transport is created, which can finish
     * asynchronously later.
     */
    PJSUA_MED_TP_CREATING,

    /** Media transport creation is completed, but not initialized yet. */
    PJSUA_MED_TP_IDLE,

    /** Initialized (media_create() has been called). */
    PJSUA_MED_TP_INIT,

    /** Running (media_start() has been called). */
    PJSUA_MED_TP_RUNNING,

    /** Disabled (transport is initialized, but media is being disabled). */
    PJSUA_MED_TP_DISABLED

} pjsua_med_tp_st;


/**
 * Structure to be passed on media transport state callback.
 */
typedef struct pjsua_med_tp_state_info
{
    /**
     * The media index.
     */
    unsigned             med_idx;

    /**
     * The media transport state
     */
    pjsua_med_tp_st      state;

    /**
     * The last error code related to the media transport state.
     */
    pj_status_t		 status;

    /**
     * Optional SIP error code.
     */
    int		         sip_err_code;

    /**
     * Optional extended info, the content is specific for each transport type.
     */
    void		*ext_info;

} pjsua_med_tp_state_info;


/**
  * Type of callback to be called when media transport state is changed.
  *
  * @param call_id	The call ID.
  * @param info         The media transport state info.
  *
  * @return		The callback must return PJ_SUCCESS at the moment.
  */
typedef pj_status_t
(*pjsua_med_tp_state_cb)(pjsua_call_id call_id,
                         const pjsua_med_tp_state_info *info);


/**
 * Typedef of callback to be registered to #pjsua_resolve_stun_servers()
 * and to be called when STUN resolution completes.
 */
typedef void (*pj_stun_resolve_cb)(const pj_stun_resolve_result *result);


/**
 * This enumeration specifies the options for custom media transport creation.
 */
typedef enum pjsua_create_media_transport_flag
{
   /**
    * This flag indicates that the media transport must also close its
    * "member" or "child" transport when pjmedia_transport_close() is
    * called. If this flag is not specified, then the media transport
    * must not call pjmedia_transport_close() of its member transport.
    */
   PJSUA_MED_TP_CLOSE_MEMBER = 1

} pjsua_create_media_transport_flag;


/**
 * Specify SRTP media transport settings.
 */
typedef struct pjsua_srtp_opt
{
    /**
     * Specify the number of crypto suite settings. If set to zero, all
     * available cryptos will be enabled. Note that available crypto names
     * can be enumerated using pjmedia_srtp_enum_crypto().
     *
     * Default is zero.
     */
    unsigned			 crypto_count;

    /**
     * Specify individual crypto suite setting and its priority order.
     *
     * Notes for DTLS-SRTP keying:
     *  - Currently only supports these cryptos: AES_CM_128_HMAC_SHA1_80,
     *    AES_CM_128_HMAC_SHA1_32, AEAD_AES_256_GCM, and AEAD_AES_128_GCM.
     *  - SRTP key is not configurable.
     */
    pjmedia_srtp_crypto		 crypto[PJMEDIA_SRTP_MAX_CRYPTOS];

    /**
     * Specify the number of enabled keying methods. If set to zero, all
     * keyings will be enabled. Maximum value is PJMEDIA_SRTP_MAX_KEYINGS.
     * Note that available keying methods can be enumerated using
     * pjmedia_srtp_enum_keying().
     *
     * Default is zero (all keyings are enabled with priority order:
     * SDES, DTLS-SRTP).
     */
    unsigned			 keying_count;

    /**
     * Specify enabled keying methods and its priority order. Keying method
     * with higher priority will be given earlier chance to process the SDP,
     * for example as currently only one keying is supported in the SDP offer,
     * keying with first priority will be likely used in the SDP offer.
     */
    pjmedia_srtp_keying_method	 keying[PJMEDIA_SRTP_KEYINGS_COUNT];

} pjsua_srtp_opt;


/**
 * This enumeration specifies the contact rewrite method.
 */
typedef enum pjsua_contact_rewrite_method
{
    /**
      * The Contact update will be done by sending unregistration
      * to the currently registered Contact, while simultaneously sending new
      * registration (with different Call-ID) for the updated Contact.
      */
    PJSUA_CONTACT_REWRITE_UNREGISTER = 1,

    /**
      * The Contact update will be done in a single, current
      * registration session, by removing the current binding (by setting its
      * Contact's expires parameter to zero) and adding a new Contact binding,
      * all done in a single request.
      */
    PJSUA_CONTACT_REWRITE_NO_UNREG = 2,

    /**
      * The Contact update will be done when receiving any registration final
      * response. If this flag is not specified, contact update will only be
      * done upon receiving 2xx response. This flag MUST be used with
      * PJSUA_CONTACT_REWRITE_UNREGISTER or PJSUA_CONTACT_REWRITE_NO_UNREG
      * above to specify how the Contact update should be performed when
      * receiving 2xx response.
      */
    PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE = 4

} pjsua_contact_rewrite_method;


/**
 * This enumeration specifies the operation when handling IP change.
 */
typedef enum pjsua_ip_change_op {
    /**
     * Hasn't start ip change process.
     */
    PJSUA_IP_CHANGE_OP_NULL,

    /**
     * The restart listener process.
     */
    PJSUA_IP_CHANGE_OP_RESTART_LIS,

    /**
     * The shutdown transport process.
     */
    PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP,

    /**
     * The update contact process.
     */
    PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT,

    /**
     * The hanging up call process.
     */
    PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS,

    /**
     * The re-INVITE call process.
     */
    PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS,

    /**
     * The ip change process has completed.
     */
    PJSUA_IP_CHANGE_OP_COMPLETED

} pjsua_ip_change_op;


/**
 * This will contain the information of the callback \a on_ip_change_progress.
 */
typedef union pjsua_ip_change_op_info {
    /**
     * The information from listener restart operation.
     */
    struct {
	int transport_id;
    } lis_restart;

    /**
     * The information from shutdown transport.
     */
    struct {
	int acc_id;
    } acc_shutdown_tp;

    /**
     * The information from updating contact.
     */
    struct {
	pjsua_acc_id acc_id;
	pj_bool_t is_register;	/**< SIP Register if PJ_TRUE.	    */
	int code;		/**< SIP status code received.	    */
    } acc_update_contact;

    /**
     * The information from hanging up call operation.
     */
    struct {
	pjsua_acc_id acc_id;
	pjsua_call_id call_id;
    } acc_hangup_calls;

    /**
     * The information from re-Invite call operation.
     */
    struct {
	pjsua_acc_id acc_id;
	pjsua_call_id call_id;
    } acc_reinvite_calls;
} pjsua_ip_change_op_info;


/**
 * This enumeration specifies DTMF method.
 */
typedef enum pjsua_dtmf_method {
    /**
     * Send DTMF using RFC2833.
     */
    PJSUA_DTMF_METHOD_RFC2833,

    /**
     * Send DTMF using SIP INFO.
     * Notes:
     * - This method is not finalized in any standard/rfc, however it is 
     *   commonly used.
     * - Warning: in case the remote doesn't support SIP INFO, response might 
     *   not be sent and the sender will deal this as timeout and disconnect
     *   the call.
     */
    PJSUA_DTMF_METHOD_SIP_INFO

} pjsua_dtmf_method;


/**
 * Constant to specify unknown duration in \a pjsua_dtmf_info and
 * \a pjsua_dtmf_event.
 */
#define PJSUA_UNKNOWN_DTMF_DURATION     ((unsigned)-1)


/**
 * This will contain the information of the callback \a on_dtmf_digit2.
 */
typedef struct pjsua_dtmf_info {
    /**
     * The method used to send DTMF.
     */
    pjsua_dtmf_method method;

    /**
     * DTMF ASCII digit.
     */    
    unsigned digit;

    /**
     * DTMF signal duration. If the duration is unknown, this value is set to
     * PJSUA_UNKNOWN_DTMF_DURATION.
     */
    unsigned duration;

} pjsua_dtmf_info;


/**
 * This will contain the information of the callback \a on_dtmf_event.
 */
typedef struct pjsua_dtmf_event {
    /**
     * The method used to send DTMF.
     */
    pjsua_dtmf_method method;

    /**
     * The timestamp identifying the begin of the event. Timestamp units are
     * expressed in milliseconds.
     * Note that this value should only be used to compare multiple events
     * received via the same method relatively to each other, as the time-base
     * is randomized.
     */
    unsigned timestamp;

    /**
     * DTMF ASCII digit.
     */
    unsigned digit;

    /**
     * DTMF signal duration in milliseconds. Interpretation of the duration
     * depends on the flag PJMEDIA_STREAM_DTMF_IS_END.
     * If PJMEDIA_STREAM_DTMF_IS_END is set, this contains the total duration
     * of the DTMF signal or PJSUA_UNKNOWN_DTMF_DURATION if the duration is
     * unknown.
     * If PJMEDIA_STREAM_DTMF_IS_END is not set, this contains the duration
     * of the DTMF signal received up to this point in time.
     * A duration of "0" indicates an infinitely long duration.
     */
    unsigned duration;

    /**
     * Flags indicating additional information about the DTMF event.
     * If PJMEDIA_STREAM_DTMF_IS_UPDATE is set, the event was already
     * indicated earlier. The new indication contains an updated event
     * duration.
     * If PJMEDIA_STREAM_DTMF_IS_END is set, the event has ended and this
     * indication contains the final event duration. Note that end
     * indications might get lost. Hence it is not guaranteed to receive
     * an event with PJMEDIA_STREAM_DTMF_IS_END for every event.
     */
    unsigned flags;
} pjsua_dtmf_event;


/**
 * Call settings.
 */
typedef struct pjsua_call_setting
{
    /**
     * Bitmask of #pjsua_call_flag constants.
     *
     * Default: PJSUA_CALL_INCLUDE_DISABLED_MEDIA
     */
    unsigned	     flag;

    /**
     * This flag controls what methods to request keyframe are allowed on
     * the call. Value is bitmask of #pjsua_vid_req_keyframe_method.
     *
     * Default: (PJSUA_VID_REQ_KEYFRAME_SIP_INFO | 
     *		 PJSUA_VID_REQ_KEYFRAME_RTCP_PLI)
     */
    unsigned	     req_keyframe_method;

    /**
     * Number of simultaneous active audio streams for this call. Setting
     * this to zero will disable audio in this call.
     *
     * Default: 1
     */
    unsigned         aud_cnt;

    /**
     * Number of simultaneous active video streams for this call. Setting
     * this to zero will disable video in this call.
     *
     * Default: 1 (if video feature is enabled, otherwise it is zero)
     */
    unsigned         vid_cnt;

    /**
     * Media direction. This setting will only be used if the flag
     * PJSUA_CALL_SET_MEDIA_DIR is set, and it will persist for subsequent
     * offers or answers. 
     * For example, a media that is set as PJMEDIA_DIR_ENCODING can only
     * mark the stream in the SDP as sendonly or inactive, but will not
     * become sendrecv in subsequent offers and answers.
     * Application can update the media direction in any API or callback
     * that accepts pjsua_call_setting as a parameter, such as via
     * pjsua_call_reinvite/update() or in on_call_rx_offer/reinvite()
     * callback.
     *
     * The index of the media dir will correspond to the provisional media
     * in pjsua_call_info.prov_media.
     * For offers that involve adding new medias (such as initial offer),
     * the index will correspond to all new audio media first, then video.
     * For example, for a new call with 2 audios and 1 video, media_dir[0]
     * and media_dir[1] will be for the audios, and media_dir[2] video.
     *
     * Default: PJMEDIA_DIR_ENCODING_DECODING
     */
    pjmedia_dir	     media_dir[PJMEDIA_MAX_SDP_MEDIA];

} pjsua_call_setting;


/**
 * This structure describes application callback to receive various event
 * notification from PJSUA-API. All of these callbacks are OPTIONAL,
 * although definitely application would want to implement some of
 * the important callbacks (such as \a on_incoming_call).
 */
typedef struct pjsua_callback
{
    /**
     * Notify application when call state has changed.
     * Application may then query the call info to get the
     * detail call states by calling  pjsua_call_get_info() function.
     *
     * @param call_id	The call index.
     * @param e		Event which causes the call state to change.
     */
    void (*on_call_state)(pjsua_call_id call_id, pjsip_event *e);

    /**
     * Notify application on incoming call.
     *
     * @param acc_id	The account which match the incoming call.
     * @param call_id	The call id that has just been created for
     *			the call.
     * @param rdata	The incoming INVITE request.
     */
    void (*on_incoming_call)(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata);

    /**
     * This is a general notification callback which is called whenever
     * a transaction within the call has changed state. Application can
     * implement this callback for example to monitor the state of
     * outgoing requests, or to answer unhandled incoming requests
     * (such as INFO) with a final response.
     *
     * @param call_id	Call identification.
     * @param tsx	The transaction which has changed state.
     * @param e		Transaction event that caused the state change.
     */
    void (*on_call_tsx_state)(pjsua_call_id call_id,
			      pjsip_transaction *tsx,
			      pjsip_event *e);

    /**
     * Notify application when media state in the call has changed.
     * Normal application would need to implement this callback, e.g.
     * to connect the call's media to sound device. When ICE is used,
     * this callback will also be called to report ICE negotiation
     * failure. When DTLS-SRTP is used, this callback will also be called
     * to report DTLS negotiation failure.
     *
     * @param call_id	The call index.
     */
    void (*on_call_media_state)(pjsua_call_id call_id);


    /**
     * Notify application when a call has just created a local SDP (for 
     * initial or subsequent SDP offer/answer). Application can implement
     * this callback to modify the SDP, before it is being sent and/or
     * negotiated with remote SDP, for example to apply per account/call
     * basis codecs priority or to add custom/proprietary SDP attributes.
     *
     * @param call_id	The call index.
     * @param sdp	The SDP has just been created.
     * @param pool	The pool instance, application should use this pool
     *			to modify the SDP.
     * @param rem_sdp	The remote SDP, will be NULL if local is SDP offerer.
     */
    void (*on_call_sdp_created)(pjsua_call_id call_id,
			        pjmedia_sdp_session *sdp,
			        pj_pool_t *pool,
			        const pjmedia_sdp_session *rem_sdp);

    /**
     * Notify application when an audio media session is about to be created
     * (as opposed to on_stream_created() and on_stream_created2() which are
     * called *after* the session has been created). The application may change
     * some stream info parameter values, i.e: jb_init, jb_min_pre, jb_max_pre,
     * jb_max, use_ka, rtcp_sdes_bye_disabled, jb_discard_algo (audio),
     * codec_param->enc_fmt (video).
     *
     * @param call_id       Call identification.
     * @param param         The on stream precreate callback parameter.
     */
    void (*on_stream_precreate)(pjsua_call_id call_id,
                                pjsua_on_stream_precreate_param *param);

    /**
     * Notify application when audio media session is created and before it is
     * registered to the conference bridge. Application may return different
     * audio media port if it has added media processing port to the stream.
     * This media port then will be added to the conference bridge instead.
     *
     * Note: if implemented, on_stream_created2() callback will be called
     * instead of this one. 
     *
     * @param call_id	    Call identification.
     * @param strm	    Audio media stream.
     * @param stream_idx    Stream index in the audio media session.
     * @param p_port	    On input, it specifies the audio media port of the
     *			    stream. Application may modify this pointer to
     *			    point to different media port to be registered
     *			    to the conference bridge.
     */
    void (*on_stream_created)(pjsua_call_id call_id,
			      pjmedia_stream *strm,
                              unsigned stream_idx,
			      pjmedia_port **p_port);

    /**
     * Notify application when audio media session is created and before it is
     * registered to the conference bridge. Application may return different
     * audio media port if it has added media processing port to the stream.
     * This media port then will be added to the conference bridge instead.
     *
     * @param call_id	    Call identification.
     * @param param	    The on stream created callback parameter.
     */
    void (*on_stream_created2)(pjsua_call_id call_id,
			       pjsua_on_stream_created_param *param);

    /**
     * Notify application when audio media session has been unregistered from
     * the conference bridge and about to be destroyed.
     *
     * @param call_id	    Call identification.
     * @param strm	    Audio media stream.
     * @param stream_idx    Stream index in the audio media session.
     */
    void (*on_stream_destroyed)(pjsua_call_id call_id,
                                pjmedia_stream *strm,
				unsigned stream_idx);

    /**
     * Notify application upon incoming DTMF digits using RFC 2833 payload 
     * formats. This callback will not be called if app implements \a
     * on_dtmf_digit2() or \a on_dtmf_event().
     *
     * @param call_id	The call index.
     * @param digit	DTMF ASCII digit.
     */
    void (*on_dtmf_digit)(pjsua_call_id call_id, int digit);

    /**
     * Notify application upon incoming DTMF digits using the method specified 
     * in \a pjsua_dtmf_method. This callback will not be called if app
     * implements \a on_dtmf_event().
     *
     * @param call_id	The call index.
     * @param info	The DTMF info.
     */
    void (*on_dtmf_digit2)(pjsua_call_id call_id, const pjsua_dtmf_info *info);

    /**
     * Notify application upon incoming DTMF digits using the method specified 
     * in \a pjsua_dtmf_method. Includes additional information about events
     * received via RTP.
     *
     * @param call_id	The call index.
     * @param event	The DTMF event.
     */
    void (*on_dtmf_event)(pjsua_call_id call_id,
                          const pjsua_dtmf_event *event);

    /**
     * Notify application on call being transferred (i.e. REFER is received).
     * Application can decide to accept/reject transfer request
     * by setting the code (default is 202). When this callback
     * is not defined, the default behavior is to accept the
     * transfer. See also on_call_transfer_request2() callback for
     * the version with \a pjsua_call_setting in the argument list.
     *
     * @param call_id	The call index.
     * @param dst	The destination where the call will be 
     *			transferred to.
     * @param code	Status code to be returned for the call transfer
     *			request. On input, it contains status code 202.
     */
    void (*on_call_transfer_request)(pjsua_call_id call_id,
				     const pj_str_t *dst,
				     pjsip_status_code *code);

    /**
     * Notify application on call being transferred (i.e. REFER is received).
     * Application can decide to accept/reject transfer request
     * by setting the code (default is 202). When this callback
     * is not defined, the default behavior is to accept the
     * transfer.
     *
     * @param call_id	The call index.
     * @param dst	The destination where the call will be 
     *			transferred to.
     * @param code	Status code to be returned for the call transfer
     *			request. On input, it contains status code 202.
     * @param opt	The current call setting, application can update
     *			this setting for the call being transferred.
     */
    void (*on_call_transfer_request2)(pjsua_call_id call_id,
				      const pj_str_t *dst,
				      pjsip_status_code *code,
				      pjsua_call_setting *opt);

    /**
     * Notify application of the status of previously sent call
     * transfer request. Application can monitor the status of the
     * call transfer request, for example to decide whether to 
     * terminate existing call.
     *
     * @param call_id	    Call ID.
     * @param st_code	    Status progress of the transfer request.
     * @param st_text	    Status progress text.
     * @param final	    If non-zero, no further notification will
     *			    be reported. The st_code specified in
     *			    this callback is the final status.
     * @param p_cont	    Initially will be set to non-zero, application
     *			    can set this to FALSE if it no longer wants
     *			    to receie further notification (for example,
     *			    after it hangs up the call).
     */
    void (*on_call_transfer_status)(pjsua_call_id call_id,
				    int st_code,
				    const pj_str_t *st_text,
				    pj_bool_t final,
				    pj_bool_t *p_cont);

    /**
     * Notify application about incoming INVITE with Replaces header.
     * Application may reject the request by setting non-2xx code.
     * See also on_call_replace_request2() callback for the version
     * with \a pjsua_call_setting in the argument list.
     *
     * @param call_id	    The call ID to be replaced.
     * @param rdata	    The incoming INVITE request to replace the call.
     * @param st_code	    Status code to be set by application. Application
     *			    should only return a final status (200-699).
     * @param st_text	    Optional status text to be set by application.
     */
    void (*on_call_replace_request)(pjsua_call_id call_id,
				    pjsip_rx_data *rdata,
				    int *st_code,
				    pj_str_t *st_text);

    /**
     * Notify application about incoming INVITE with Replaces header.
     * Application may reject the request by setting non-2xx code.
     *
     * @param call_id	    The call ID to be replaced.
     * @param rdata	    The incoming INVITE request to replace the call.
     * @param st_code	    Status code to be set by application. Application
     *			    should only return a final status (200-699).
     * @param st_text	    Optional status text to be set by application.
     * @param opt	    The current call setting, application can update
     *			    this setting for the call being replaced.
     */
    void (*on_call_replace_request2)(pjsua_call_id call_id,
				     pjsip_rx_data *rdata,
				     int *st_code,
				     pj_str_t *st_text,
				     pjsua_call_setting *opt);

    /**
     * Notify application that an existing call has been replaced with
     * a new call. This happens when PJSUA-API receives incoming INVITE
     * request with Replaces header.
     *
     * After this callback is called, normally PJSUA-API will disconnect
     * \a old_call_id and establish \a new_call_id.
     *
     * @param old_call_id   Existing call which to be replaced with the
     *			    new call.
     * @param new_call_id   The new call.
     * @param rdata	    The incoming INVITE with Replaces request.
     */
    void (*on_call_replaced)(pjsua_call_id old_call_id,
			     pjsua_call_id new_call_id);


    /**
     * Notify application when call has received new offer from remote
     * (i.e. re-INVITE/UPDATE with SDP is received, or from the
     * INVITE response in the case that the initial outgoing INVITE
     * has no SDP). Application can
     * decide to accept/reject the offer by setting the code (default
     * is 200). If the offer is accepted, application can update the 
     * call setting to be applied in the answer. When this callback is
     * not defined, the default behavior is to accept the offer using
     * current call setting.
     *
     * Note: this callback may not be called if \a on_call_rx_reinvite()
     * is implemented.
     *
     * @param call_id	The call index.
     * @param offer	The new offer received.
     * @param reserved	Reserved param, currently not used.
     * @param code	Status code to be returned for answering the
     *			offer. On input, it contains status code 200.
     *			Currently, valid values are only 200 and 488.
     * @param opt	The current call setting, application can update
     *			this setting for answering the offer.
     */
    void (*on_call_rx_offer)(pjsua_call_id call_id,
			     const pjmedia_sdp_session *offer,
			     void *reserved,
			     pjsip_status_code *code,
			     pjsua_call_setting *opt);


    /**
     * Notify application when call has received a re-INVITE with offer
     * from the peer. It allows more fine-grained control over the response
     * to a re-INVITE. If application sets async to PJ_TRUE, it can send
     * the reply manually using the function #pjsua_call_answer_with_sdp().
     * Otherwise, by default the re-INVITE will be answered automatically
     * after the callback returns.
     *
     * Currently, this callback is only called for re-INVITE with
     * SDP, but app should be prepared to handle the case of re-INVITE
     * without SDP.
     *
     * Remarks: If manually answering at a later timing, application may
     * need to monitor on_call_tsx_state() callback to check whether
     * the re-INVITE is already answered automatically with 487 due to
     * being cancelled.
     *
     * Note: on_call_rx_offer() will still be called after this callback,
     * but only if async is PJ_FALSE and code is 200. 
     *
     * @param call_id	The call index.
     * @param offer	Remote offer.
     * @param rdata     The received re-INVITE request.
     * @param reserved	Reserved param, currently not used.
     * @param async	On input, it is PJ_FALSE. Set to PJ_TRUE if
     *			app wants to manually answer the re-INVITE.
     * @param code	Status code to be returned for answering the
     *			offer. On input, it contains status code 200.
     *			Currently, valid values are only 200 and 488.
     * @param opt	The current call setting, application can update
     *			this setting for answering the offer.
     */
    void (*on_call_rx_reinvite)(pjsua_call_id call_id,
    		                const pjmedia_sdp_session *offer,
                                pjsip_rx_data *rdata,
			     	void *reserved,
			     	pj_bool_t *async,
			     	pjsip_status_code *code,
			     	pjsua_call_setting *opt);


    /**
    * Notify application when call has received INVITE with no SDP offer.
    * Application can update the call setting (e.g: add audio/video), or
    * enable/disable codecs, or update other media session settings from
    * within the callback, however, as mandated by the standard (RFC3261
    * section 14.2), it must ensure that the update overlaps with the
    * existing media session (in codecs, transports, or other parameters)
    * that require support from the peer, this is to avoid the need for
    * the peer to reject the offer.
    *
    * When this callback is not defined, the default behavior is to send
    * SDP offer using current active media session (with all enabled codecs
    * on each media type).
    *
    * @param call_id	The call index.
    * @param reserved	Reserved param, currently not used.
    * @param opt	The current call setting, application can update
    *			this setting for generating the offer.
    */
    void (*on_call_tx_offer)(pjsua_call_id call_id,
			     void *reserved,
			     pjsua_call_setting *opt);


    /**
     * Notify application when registration or unregistration has been
     * initiated. Note that this only notifies the initial registration
     * and unregistration. Once registration session is active, subsequent
     * refresh will not cause this callback to be called.
     *
     * @param acc_id	    The account ID.
     * @param renew	    Non-zero for registration and zero for
     * 			    unregistration.
     */
    void (*on_reg_started)(pjsua_acc_id acc_id, pj_bool_t renew);

    /**
     * This is the alternative version of the \a on_reg_started() callback with
     * \a pjsua_reg_info argument.
     *
     * @param acc_id	    The account ID.
     * @param info	    The registration info.
     */
    void (*on_reg_started2)(pjsua_acc_id acc_id, 
			    pjsua_reg_info *info);
    
    /**
     * Notify application when registration status has changed.
     * Application may then query the account info to get the
     * registration details.
     *
     * @param acc_id	    The account ID.
     */
    void (*on_reg_state)(pjsua_acc_id acc_id);

    /**
     * Notify application when registration status has changed.
     * Application may inspect the registration info to get the
     * registration status details.
     *
     * @param acc_id	    The account ID.
     * @param info	    The registration info.
     */
    void (*on_reg_state2)(pjsua_acc_id acc_id, pjsua_reg_info *info);

    /**
     * Notification when incoming SUBSCRIBE request is received. Application
     * may use this callback to authorize the incoming subscribe request
     * (e.g. ask user permission if the request should be granted).
     *
     * If this callback is not implemented, all incoming presence subscription
     * requests will be accepted.
     *
     * If this callback is implemented, application has several choices on
     * what to do with the incoming request:
     *	- it may reject the request immediately by specifying non-200 class
     *    final response in the \a code argument.
     *	- it may immediately accept the request by specifying 200 as the
     *	  \a code argument. This is the default value if application doesn't
     *	  set any value to the \a code argument. In this case, the library
     *	  will automatically send NOTIFY request upon returning from this
     *	  callback.
     *  - it may delay the processing of the request, for example to request
     *    user permission whether to accept or reject the request. In this 
     *	  case, the application MUST set the \a code argument to 202, then
     *    IMMEDIATELY calls #pjsua_pres_notify() with state
     *    PJSIP_EVSUB_STATE_PENDING and later calls #pjsua_pres_notify()
     *    again to accept or reject the subscription request.
     *
     * Any \a code other than 200 and 202 will be treated as 200.
     *
     * Application MUST return from this callback immediately (e.g. it must
     * not block in this callback while waiting for user confirmation).
     *
     * @param srv_pres	    Server presence subscription instance. If
     *			    application delays the acceptance of the request,
     *			    it will need to specify this object when calling
     *			    #pjsua_pres_notify().
     * @param acc_id	    Account ID most appropriate for this request.
     * @param buddy_id	    ID of the buddy matching the sender of the
     *			    request, if any, or PJSUA_INVALID_ID if no
     *			    matching buddy is found.
     * @param from	    The From URI of the request.
     * @param rdata	    The incoming request.
     * @param code	    The status code to respond to the request. The
     *			    default value is 200. Application may set this
     *			    to other final status code to accept or reject
     *			    the request.
     * @param reason	    The reason phrase to respond to the request.
     * @param msg_data	    If the application wants to send additional
     *			    headers in the response, it can put it in this
     *			    parameter.
     */
    void (*on_incoming_subscribe)(pjsua_acc_id acc_id,
				  pjsua_srv_pres *srv_pres,
				  pjsua_buddy_id buddy_id,
				  const pj_str_t *from,
				  pjsip_rx_data *rdata,
				  pjsip_status_code *code,
				  pj_str_t *reason,
				  pjsua_msg_data *msg_data);

    /**
     * Notification when server side subscription state has changed.
     * This callback is optional as application normally does not need
     * to do anything to maintain server side presence subscription.
     *
     * @param acc_id	    The account ID.
     * @param srv_pres	    Server presence subscription object.
     * @param remote_uri    Remote URI string.
     * @param state	    New subscription state.
     * @param event	    PJSIP event that triggers the state change.
     */
    void (*on_srv_subscribe_state)(pjsua_acc_id acc_id,
				   pjsua_srv_pres *srv_pres,
				   const pj_str_t *remote_uri,
				   pjsip_evsub_state state,
				   pjsip_event *event);

    /**
     * Notify application when the buddy state has changed.
     * Application may then query the buddy into to get the details.
     *
     * @param buddy_id	    The buddy id.
     */
    void (*on_buddy_state)(pjsua_buddy_id buddy_id);


    /**
     * Notify application when the state of client subscription session
     * associated with a buddy has changed. Application may use this
     * callback to retrieve more detailed information about the state
     * changed event.
     *
     * @param buddy_id	    The buddy id.
     * @param sub	    Event subscription session.
     * @param event	    The event which triggers state change event.
     */
    void (*on_buddy_evsub_state)(pjsua_buddy_id buddy_id,
				 pjsip_evsub *sub,
				 pjsip_event *event);

    /**
     * Notify application on incoming pager (i.e. MESSAGE request).
     * Argument call_id will be -1 if MESSAGE request is not related to an
     * existing call.
     *
     * See also \a on_pager2() callback for the version with \a pjsip_rx_data
     * passed as one of the argument.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param from	    URI of the sender.
     * @param to	    URI of the destination message.
     * @param contact	    The Contact URI of the sender, if present.
     * @param mime_type	    MIME type of the message.
     * @param body	    The message content.
     */
    void (*on_pager)(pjsua_call_id call_id, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *body);

    /**
     * This is the alternative version of the \a on_pager() callback with
     * \a pjsip_rx_data argument.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param from	    URI of the sender.
     * @param to	    URI of the destination message.
     * @param contact	    The Contact URI of the sender, if present.
     * @param mime_type	    MIME type of the message.
     * @param body	    The message content.
     * @param rdata	    The incoming MESSAGE request.
     * @param acc_id	    Account ID most suitable for this message.
     */
    void (*on_pager2)(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      const pj_str_t *mime_type, const pj_str_t *body,
		      pjsip_rx_data *rdata, pjsua_acc_id acc_id);

    /**
     * Notify application about the delivery status of outgoing pager
     * request. See also on_pager_status2() callback for the version with
     * \a pjsip_rx_data in the argument list.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param to	    Destination URI.
     * @param body	    Message body.
     * @param user_data	    Arbitrary data that was specified when sending
     *			    IM message.
     * @param status	    Delivery status.
     * @param reason	    Delivery status reason.
     */
    void (*on_pager_status)(pjsua_call_id call_id,
			    const pj_str_t *to,
			    const pj_str_t *body,
			    void *user_data,
			    pjsip_status_code status,
			    const pj_str_t *reason);

    /**
     * Notify application about the delivery status of outgoing pager
     * request.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param to	    Destination URI.
     * @param body	    Message body.
     * @param user_data	    Arbitrary data that was specified when sending
     *			    IM message.
     * @param status	    Delivery status.
     * @param reason	    Delivery status reason.
     * @param tdata	    The original MESSAGE request.
     * @param rdata	    The incoming MESSAGE response, or NULL if the
     *			    message transaction fails because of time out 
     *			    or transport error.
     * @param acc_id	    Account ID from this the instant message was
     *			    send.
     */
    void (*on_pager_status2)(pjsua_call_id call_id,
			     const pj_str_t *to,
			     const pj_str_t *body,
			     void *user_data,
			     pjsip_status_code status,
			     const pj_str_t *reason,
			     pjsip_tx_data *tdata,
			     pjsip_rx_data *rdata,
			     pjsua_acc_id acc_id);

    /**
     * Notify application about typing indication.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param from	    URI of the sender.
     * @param to	    URI of the destination message.
     * @param contact	    The Contact URI of the sender, if present.
     * @param is_typing	    Non-zero if peer is typing, or zero if peer
     *			    has stopped typing a message.
     */
    void (*on_typing)(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      pj_bool_t is_typing);

    /**
     * Notify application about typing indication.
     *
     * @param call_id	    Containts the ID of the call where the IM was
     *			    sent, or PJSUA_INVALID_ID if the IM was sent
     *			    outside call context.
     * @param from	    URI of the sender.
     * @param to	    URI of the destination message.
     * @param contact	    The Contact URI of the sender, if present.
     * @param is_typing	    Non-zero if peer is typing, or zero if peer
     *			    has stopped typing a message.
     * @param rdata	    The received request.
     * @param acc_id	    Account ID most suitable for this message.
     */
    void (*on_typing2)(pjsua_call_id call_id, const pj_str_t *from,
		       const pj_str_t *to, const pj_str_t *contact,
		       pj_bool_t is_typing, pjsip_rx_data *rdata,
		       pjsua_acc_id acc_id);

    /**
     * Callback when the library has finished performing NAT type
     * detection.
     *
     * @param res	    NAT detection result.
     */
    void (*on_nat_detect)(const pj_stun_nat_detect_result *res);

    /**
     * This callback is called when the call is about to resend the 
     * INVITE request to the specified target, following the previously
     * received redirection response.
     *
     * Application may accept the redirection to the specified target,
     * reject this target only and make the session continue to try the next 
     * target in the list if such target exists, stop the whole
     * redirection process altogether and cause the session to be
     * disconnected, or defer the decision to ask for user confirmation.
     *
     * This callback is optional. If this callback is not implemented,
     * the default behavior is to NOT follow the redirection response.
     *
     * @param call_id	The call ID.
     * @param target	The current target to be tried.
     * @param e		The event that caused this callback to be called.
     *			This could be the receipt of 3xx response, or
     *			4xx/5xx response received for the INVITE sent to
     *			subsequent targets, or NULL if this callback is
     *			called from within #pjsua_call_process_redirect()
     *			context.
     *
     * @return		Action to be performed for the target. Set this
     *			parameter to one of the value below:
     *			- PJSIP_REDIRECT_ACCEPT: immediately accept the
     *			  redirection. When set, the call will immediately
     *			  resend INVITE request to the target.
     *			- PJSIP_REDIRECT_ACCEPT_REPLACE: immediately accept
     *			  the redirection and replace the To header with the
     *			  current target. When set, the call will immediately
     *			  resend INVITE request to the target.
     *			- PJSIP_REDIRECT_REJECT: immediately reject this
     *			  target. The call will continue retrying with
     *			  next target if present, or disconnect the call
     *			  if there is no more target to try.
     *			- PJSIP_REDIRECT_STOP: stop the whole redirection
     *			  process and immediately disconnect the call. The
     *			  on_call_state() callback will be called with
     *			  PJSIP_INV_STATE_DISCONNECTED state immediately
     *			  after this callback returns.
     *			- PJSIP_REDIRECT_PENDING: set to this value if
     *			  no decision can be made immediately (for example
     *			  to request confirmation from user). Application
     *			  then MUST call #pjsua_call_process_redirect()
     *			  to either accept or reject the redirection upon
     *			  getting user decision.
     */
    pjsip_redirect_op (*on_call_redirected)(pjsua_call_id call_id, 
					    const pjsip_uri *target,
					    const pjsip_event *e);

    /**
     * This callback is called when message waiting indication subscription
     * state has changed. Application can then query the subscription state
     * by calling #pjsip_evsub_get_state().
     *
     * @param acc_id	The account ID.
     * @param evsub	The subscription instance.
     */
    void (*on_mwi_state)(pjsua_acc_id acc_id, pjsip_evsub *evsub);

    /**
     * This callback is called when a NOTIFY request for message summary / 
     * message waiting indication is received.
     *
     * @param acc_id	The account ID.
     * @param mwi_info	Structure containing details of the event,
     *			including the received NOTIFY request in the
     *			\a rdata field.
     */
    void (*on_mwi_info)(pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info);

    /**
     * This callback is called when transport state is changed. See also
     * #pjsip_tp_state_callback.
     */
    pjsip_tp_state_callback on_transport_state;

    /**
     * This callback is called when media transport state is changed. See
     * also #pjsua_med_tp_state_cb.
     */
    pjsua_med_tp_state_cb on_call_media_transport_state;

    /**
     * This callback is called to report error in ICE media transport.
     * Currently it is used to report TURN Refresh error.
     *
     * @param index	Transport index.
     * @param op	Operation which trigger the failure.
     * @param status	Error status.
     * @param param	Additional info about the event. Currently this will
     * 			always be set to NULL.
     */
    void (*on_ice_transport_error)(int index, pj_ice_strans_op op,
				   pj_status_t status, void *param);

    /**
     * Callback when the sound device is about to be opened or closed.
     * This callback will be called even when null sound device or no
     * sound device is configured by the application (i.e. the
     * #pjsua_set_null_snd_dev() and #pjsua_set_no_snd_dev() APIs).
     * Application can use the API #pjsua_get_snd_dev() to get the info
     * about which sound device is going to be opened/closed.
     *
     * This callback is mostly useful when the application wants to manage
     * the sound device by itself (i.e. with #pjsua_set_no_snd_dev()),
     * to get notified when it should open or close the sound device.
     *
     * @param operation	The value will be set to 0 to signal that sound
     * 			device is about to be closed, and 1 to be opened.
     *
     * @return		The callback must return PJ_SUCCESS at the moment.
     */
    pj_status_t (*on_snd_dev_operation)(int operation);

    /**
     * Notification about media events such as video notifications. This
     * callback will most likely be called from media threads, thus
     * application must not perform heavy processing in this callback.
     * Especially, application must not destroy the call or media in this
     * callback. If application needs to perform more complex tasks to
     * handle the event, it should post the task to another thread.
     *
     * @param call_id	The call id.
     * @param med_idx	The media stream index.
     * @param event 	The media event.
     */
    void (*on_call_media_event)(pjsua_call_id call_id,
				unsigned med_idx,
				pjmedia_event *event);

    /**
     * This callback can be used by application to implement custom media
     * transport adapter for the call, or to replace the media transport
     * with something completely new altogether.
     *
     * This callback is called when a new call is created. The library has
     * created a media transport for the call, and it is provided as the
     * \a base_tp argument of this callback. Upon returning, the callback
     * must return an instance of media transport to be used by the call.
     *
     * @param call_id       Call ID
     * @param media_idx     The media index in the SDP for which this media
     *                      transport will be used.
     * @param base_tp       The media transport which otherwise will be
     *                      used by the call has this callback not been
     *                      implemented.
     * @param flags         Bitmask from pjsua_create_media_transport_flag.
     *
     * @return              The callback must return an instance of media
     *                      transport to be used by the call.
     */
    pjmedia_transport* (*on_create_media_transport)(pjsua_call_id call_id,
                                                    unsigned media_idx,
                                                    pjmedia_transport *base_tp,
                                                    unsigned flags);

    /**
     * Warning: deprecated and may be removed in future release. Application
     * can set SRTP crypto settings (including keys) and keying methods
     * via pjsua_srtp_opt in pjsua_config and pjsua_acc_config.
     * See also ticket #2100.
     *
     * This callback is called before SRTP media transport is created.
     * Application can modify the SRTP setting \a srtp_opt to specify
     * the cryptos & keys and keying methods which are going to be used.
     * Note that only some fields of pjmedia_srtp_setting can be overriden
     * from this callback, i.e: "crypto_count", "crypto", "keying_count",
     * "keying", and "use" (only for initial INVITE), any modification in
     * other fields will be ignored.
     *
     * @param call_id       Call ID
     * @param media_idx     The media index in the SDP for which this SRTP
     * 			    media transport will be used.
     * @param srtp_opt      The SRTP setting. Application can modify this.
     */
    void (*on_create_media_transport_srtp)(pjsua_call_id call_id,
                                           unsigned media_idx,
                                           pjmedia_srtp_setting *srtp_opt);

    /**
     * This callback can be used by application to override the account
     * to be used to handle an incoming message. Initially, the account to
     * be used will be calculated automatically by the library. This initial
     * account will be used if application does not implement this callback,
     * or application sets an invalid account upon returning from this
     * callback.
     *
     * Note that currently the incoming messages requiring account assignment
     * are INVITE, MESSAGE, SUBSCRIBE, and unsolicited NOTIFY. This callback
     * may be called before the callback of the SIP event itself, i.e:
     * incoming call, pager, subscription, or unsolicited-event.
     *
     * @param rdata	The incoming message.
     * @param acc_id 	On input, initial account ID calculated automatically
     *			by the library. On output, the account ID prefered
     *			by application to handle the incoming message.
     */
    void (*on_acc_find_for_incoming)(const pjsip_rx_data *rdata,
				     pjsua_acc_id* acc_id);

    /**
     * Calling #pjsua_init() will initiate an async process to resolve and
     * contact each of the STUN server entries to find which is usable.
     * This callback is called when the process is complete, and can be
     * used by the application to start creating and registering accounts.
     * This way, the accounts can avoid call setup delay caused by pending
     * STUN resolution.
     *
     * See also #pj_stun_resolve_cb.
     */
    pj_stun_resolve_cb on_stun_resolution_complete;

    /** 
     * Calling #pjsua_handle_ip_change() may involve different operation. This 
     * callback is called to report the progress of each enabled operation.
     *
     * @param op	The operation.
     * @param status	The status of operation.
     * @param info	The info from the operation
     * 
     */
    void (*on_ip_change_progress)(pjsua_ip_change_op op,
				  pj_status_t status,
				  const pjsua_ip_change_op_info *info);

    /**
     * Notification about media events such as video notifications. This
     * callback will most likely be called from media threads, thus
     * application must not perform heavy processing in this callback.
     * If application needs to perform more complex tasks to handle
     * the event, it should post the task to another thread.
     *
     * @param event 	The media event.
     */
    void (*on_media_event)(pjmedia_event *event);

} pjsua_callback;


/**
 * This enumeration specifies the usage of SIP Session Timers extension.
 */
typedef enum pjsua_sip_timer_use
{
    /**
     * When this flag is specified, Session Timers will not be used in any
     * session, except it is explicitly required in the remote request.
     */
    PJSUA_SIP_TIMER_INACTIVE,

    /**
     * When this flag is specified, Session Timers will be used in all 
     * sessions whenever remote supports and uses it.
     */
    PJSUA_SIP_TIMER_OPTIONAL,

    /**
     * When this flag is specified, Session Timers support will be 
     * a requirement for the remote to be able to establish a session.
     */
    PJSUA_SIP_TIMER_REQUIRED,

    /**
     * When this flag is specified, Session Timers will always be used
     * in all sessions, regardless whether remote supports/uses it or not.
     */
    PJSUA_SIP_TIMER_ALWAYS

} pjsua_sip_timer_use;


/**
 * This constants controls the use of 100rel extension.
 */
typedef enum pjsua_100rel_use
{
    /**
     * Not used. For UAC, support for 100rel will be indicated in Supported
     * header so that peer can opt to use it if it wants to. As UAS, this
     * option will NOT cause 100rel to be used even if UAC indicates that
     * it supports this feature.
     */
    PJSUA_100REL_NOT_USED,

    /**
     * Mandatory. UAC will place 100rel in Require header, and UAS will
     * reject incoming calls unless it has 100rel in Supported header.
     */
    PJSUA_100REL_MANDATORY,

    /**
     * Optional. Similar to PJSUA_100REL_NOT_USED, except that as UAS, this
     * option will cause 100rel to be used if UAC indicates that it supports it.
     */
    PJSUA_100REL_OPTIONAL

} pjsua_100rel_use;


/**
 * This structure describes the settings to control the API and
 * user agent behavior, and can be specified when calling #pjsua_init().
 * Before setting the values, application must call #pjsua_config_default()
 * to initialize this structure with the default values.
 */
typedef struct pjsua_config
{

    /** 
     * Maximum calls to support (default: 4). The value specified here
     * must be smaller than or equal to the compile time maximum settings
     * PJSUA_MAX_CALLS. To increase this limit, the library must be
     * recompiled with new PJSUA_MAX_CALLS value.
     */
    unsigned	    max_calls;

    /** 
     * Number of worker threads. Normally application will want to have at
     * least one worker thread, unless when it wants to poll the library
     * periodically, which in this case the worker thread can be set to
     * zero.
     */
    unsigned	    thread_cnt;

    /**
     * Number of nameservers. If no name server is configured, the SIP SRV
     * resolution would be disabled, and domain will be resolved with
     * standard pj_gethostbyname() function.
     */
    unsigned	    nameserver_count;

    /**
     * Array of nameservers to be used by the SIP resolver subsystem.
     * The order of the name server specifies the priority (first name
     * server will be used first, unless it is not reachable).
     */
    pj_str_t	    nameserver[4];

    /**
     * Force loose-route to be used in all route/proxy URIs (outbound_proxy
     * and account's proxy settings). When this setting is enabled, the
     * library will check all the route/proxy URIs specified in the settings
     * and append ";lr" parameter to the URI if the parameter is not present.
     *
     * Default: 1
     */
    pj_bool_t	    force_lr;

    /**
     * Number of outbound proxies in the \a outbound_proxy array.
     */
    unsigned	    outbound_proxy_cnt;

    /** 
     * Specify the URL of outbound proxies to visit for all outgoing requests.
     * The outbound proxies will be used for all accounts, and it will
     * be used to build the route set for outgoing requests. The final
     * route set for outgoing requests will consists of the outbound proxies
     * and the proxy configured in the account.
     */
    pj_str_t	    outbound_proxy[4];

    /**
     * Warning: deprecated, please use \a stun_srv field instead. To maintain
     * backward compatibility, if \a stun_srv_cnt is zero then the value of
     * this field will be copied to \a stun_srv field, if present.
     *
     * Specify domain name to be resolved with DNS SRV resolution to get the
     * address of the STUN server. Alternatively application may specify
     * \a stun_host instead.
     *
     * If DNS SRV resolution failed for this domain, then DNS A resolution
     * will be performed only if \a stun_host is specified.
     */
    pj_str_t	    stun_domain;

    /**
     * Warning: deprecated, please use \a stun_srv field instead. To maintain
     * backward compatibility, if \a stun_srv_cnt is zero then the value of
     * this field will be copied to \a stun_srv field, if present.
     *
     * Specify STUN server to be used, in "HOST[:PORT]" format. If port is
     * not specified, default port 3478 will be used.
     */
    pj_str_t	    stun_host;

    /**
     * Number of STUN server entries in \a stun_srv array.
     */
    unsigned	    stun_srv_cnt;

    /**
     * Array of STUN servers to try. The library will try to resolve and
     * contact each of the STUN server entry until it finds one that is
     * usable. Each entry may be a domain name, host name, IP address, and
     * it may contain an optional port number. For example:
     *	- "pjsip.org" (domain name)
     *	- "sip.pjsip.org" (host name)
     *	- "pjsip.org:33478" (domain name and a non-standard port number)
     *	- "10.0.0.1:3478" (IP address and port number)
     *
     * When nameserver is configured in the \a pjsua_config.nameserver field,
     * if entry is not an IP address, it will be resolved with DNS SRV 
     * resolution first, and it will fallback to use DNS A resolution if this
     * fails. Port number may be specified even if the entry is a domain name,
     * in case the DNS SRV resolution should fallback to a non-standard port.
     *
     * When nameserver is not configured, entries will be resolved with
     * #pj_gethostbyname() if it's not an IP address. Port number may be
     * specified if the server is not listening in standard STUN port.
     */
    pj_str_t	    stun_srv[8];

    /**
     * This specifies if the library should try to do an IPv6 resolution of
     * the STUN servers if the IPv4 resolution fails. It can be useful
     * in an IPv6-only environment, including on NAT64.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t	    stun_try_ipv6;

    /**
     * This specifies if the library should ignore failure with the
     * STUN servers. If this is set to PJ_FALSE, the library will refuse to
     * start if it fails to resolve or contact any of the STUN servers.
     *
     * This setting will also determine what happens if STUN servers are
     * unavailable during runtime (if set to PJ_FALSE, calls will
     * directly fail, otherwise (if PJ_TRUE) call medias will
     * fallback to proceed as though not using STUN servers.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t	    stun_ignore_failure;

    /**
     * This specifies whether STUN requests for resolving socket mapped
     * address should use the new format, i.e: having STUN magic cookie
     * in its transaction ID.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t	    stun_map_use_stun2;

    /**
     * Support for adding and parsing NAT type in the SDP to assist 
     * troubleshooting. The valid values are:
     *	- 0: no information will be added in SDP, and parsing is disabled.
     *	- 1: only the NAT type number is added.
     *	- 2: add both NAT type number and name.
     *
     * Default: 1
     */
    int		    nat_type_in_sdp;

    /**
     * Specify how the support for reliable provisional response (100rel/
     * PRACK) should be used by default. Note that this setting can be
     * further customized in account configuration (#pjsua_acc_config).
     *
     * Default: PJSUA_100REL_NOT_USED
     */
    pjsua_100rel_use require_100rel;

    /**
     * Specify the usage of Session Timers for all sessions. See the
     * #pjsua_sip_timer_use for possible values. Note that this setting can be
     * further customized in account configuration (#pjsua_acc_config).
     *
     * Default: PJSUA_SIP_TIMER_OPTIONAL
     */
    pjsua_sip_timer_use use_timer;

    /**
     * Handle unsolicited NOTIFY requests containing message waiting 
     * indication (MWI) info. Unsolicited MWI is incoming NOTIFY requests 
     * which are not requested by client with SUBSCRIBE request. 
     *
     * If this is enabled, the library will respond 200/OK to the NOTIFY
     * request and forward the request to \a on_mwi_info() callback.
     *
     * See also \a mwi_enabled field #on pjsua_acc_config.
     *
     * Default: PJ_TRUE
     *
     */
    pj_bool_t	    enable_unsolicited_mwi;

    /**
     * Specify Session Timer settings, see #pjsip_timer_setting. 
     * Note that this setting can be further customized in account 
     * configuration (#pjsua_acc_config).
     */
    pjsip_timer_setting timer_setting;

    /** 
     * Number of credentials in the credential array.
     */
    unsigned	    cred_count;

    /** 
     * Array of credentials. These credentials will be used by all accounts,
     * and can be used to authenticate against outbound proxies. If the
     * credential is specific to the account, then application should set
     * the credential in the pjsua_acc_config rather than the credential
     * here.
     */
    pjsip_cred_info cred_info[PJSUA_ACC_MAX_PROXIES];

    /**
     * Application callback to receive various event notifications from
     * the library.
     */
    pjsua_callback  cb;

    /**
     * Optional user agent string (default empty). If it's empty, no
     * User-Agent header will be sent with outgoing requests.
     */
    pj_str_t	    user_agent;

    /**
     * Specify default value of secure media transport usage. 
     * Valid values are PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, and
     * PJMEDIA_SRTP_MANDATORY.
     *
     * Note that this setting can be further customized in account 
     * configuration (#pjsua_acc_config).
     *
     * Default: #PJSUA_DEFAULT_USE_SRTP
     */
    pjmedia_srtp_use	use_srtp;

    /**
     * Specify whether SRTP requires secure signaling to be used. This option
     * is only used when \a use_srtp option above is non-zero.
     *
     * Valid values are:
     *	0: SRTP does not require secure signaling
     *	1: SRTP requires secure transport such as TLS
     *	2: SRTP requires secure end-to-end transport (SIPS)
     *
     * Note that this setting can be further customized in account 
     * configuration (#pjsua_acc_config).
     *
     * Default: #PJSUA_DEFAULT_SRTP_SECURE_SIGNALING
     */
    int		     srtp_secure_signaling;

    /**
     * This setting has been deprecated and will be ignored.
     */
    pj_bool_t	     srtp_optional_dup_offer;

    /**
     * Specify SRTP transport setting. Application can initialize it with
     * default values using pjsua_srtp_opt_default().
     */
    pjsua_srtp_opt   srtp_opt;

    /**
     * Disconnect other call legs when more than one 2xx responses for 
     * outgoing INVITE are received due to forking. Currently the library
     * is not able to handle simultaneous forked media, so disconnecting
     * the other call legs is necessary. 
     *
     * With this setting enabled, the library will handle only one of the
     * connected call leg, and the other connected call legs will be
     * disconnected. 
     *
     * Default: PJ_TRUE (only disable this setting for testing purposes).
     */
    pj_bool_t	     hangup_forked_call;

    /**
     * Specify whether to enable UPnP.
     *
     * Note that this setting can be further customized in account
     * configuration (#pjsua_acc_config).
     *
     * Default: PJ_FALSE
     */
    pj_bool_t        enable_upnp;

    /**
     * Specify which interface to use for UPnP. If empty, UPnP will use
     * the first suitable interface found.
     *
     * Note that this setting is only applicable if UPnP is enabled and
     * the string must be NULL terminated.
     *
     * Default: empty string
     */
    pj_str_t         upnp_if_name;

} pjsua_config;


/**
 * Flags to be given to pjsua_destroy2()
 */
typedef enum pjsua_destroy_flag
{
    /**
     * Allow sending outgoing messages (such as unregistration, event
     * unpublication, BYEs, unsubscription, etc.), but do not wait for
     * responses. This is useful to perform "best effort" clean up
     * without delaying the shutdown process waiting for responses.
     */
    PJSUA_DESTROY_NO_RX_MSG = 1,

    /**
     * If this flag is set, do not send any outgoing messages at all.
     * This flag is useful if application knows that the network which
     * the messages are to be sent on is currently down.
     */
    PJSUA_DESTROY_NO_TX_MSG = 2,

    /**
     * Do not send or receive messages during destroy. This flag is
     * shorthand for  PJSUA_DESTROY_NO_RX_MSG + PJSUA_DESTROY_NO_TX_MSG.
     */
    PJSUA_DESTROY_NO_NETWORK = PJSUA_DESTROY_NO_RX_MSG |
			       PJSUA_DESTROY_NO_TX_MSG

} pjsua_destroy_flag;

/**
 * Use this function to initialize pjsua config.
 *
 * @param cfg	pjsua config to be initialized.
 */
PJ_DECL(void) pjsua_config_default(pjsua_config *cfg);


/** The implementation has been moved to sip_auth.h */
#define pjsip_cred_dup	pjsip_cred_info_dup


/**
 * Duplicate pjsua_config.
 *
 * @param pool	    The pool to get memory from.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_config_dup(pj_pool_t *pool,
			       pjsua_config *dst,
			       const pjsua_config *src);


/**
 * This structure describes additional information to be sent with
 * outgoing SIP message. It can (optionally) be specified for example
 * with #pjsua_call_make_call(), #pjsua_call_answer(), #pjsua_call_hangup(),
 * #pjsua_call_set_hold(), #pjsua_call_send_im(), and many more.
 *
 * Application MUST call #pjsua_msg_data_init() to initialize this
 * structure before setting its values.
 */
struct pjsua_msg_data
{
    /**
     * Optional remote target URI (i.e. Target header). If NULL, the target
     * will be set to the remote URI (To header). This field is used by
     * pjsua_call_make_call(), pjsua_im_send(), pjsua_call_reinvite(),
     * pjsua_call_set_hold(), and pjsua_call_update().
     */
    pj_str_t    target_uri;

    /**
     * Additional message headers as linked list. Application can add
     * headers to the list by creating the header, either from the heap/pool
     * or from temporary local variable, and add the header using
     * linked list operation. See pjsua_app.c for some sample codes.
     */
    pjsip_hdr	hdr_list;

    /**
     * MIME type of optional message body. 
     */
    pj_str_t	content_type;

    /**
     * Optional message body to be added to the message, only when the
     * message doesn't have a body.
     */
    pj_str_t	msg_body;

    /**
     * Content type of the multipart body. If application wants to send
     * multipart message bodies, it puts the parts in \a parts and set
     * the content type in \a multipart_ctype. If the message already
     * contains a body, the body will be added to the multipart bodies.
     */
    pjsip_media_type  multipart_ctype;

    /**
     * List of multipart parts. If application wants to send multipart
     * message bodies, it puts the parts in \a parts and set the content
     * type in \a multipart_ctype. If the message already contains a body,
     * the body will be added to the multipart bodies.
     */
    pjsip_multipart_part multipart_parts;
};


/**
 * Initialize message data.
 *
 * @param msg_data  Message data to be initialized.
 */
PJ_DECL(void) pjsua_msg_data_init(pjsua_msg_data *msg_data);


/**
 * Clone message data.
 *
 * @param pool	    Pool to allocate memory for the new message data.
 * @param rhs       Message data to be cloned.
 *
 * @return          The new message data.
 */
PJ_DECL(pjsua_msg_data*) pjsua_msg_data_clone(pj_pool_t *pool,
                                              const pjsua_msg_data *rhs);


/**
 * Instantiate pjsua application. Application must call this function before
 * calling any other functions, to make sure that the underlying libraries
 * are properly initialized. Once this function has returned success,
 * application must call pjsua_destroy() before quitting.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_create(void);


/** Forward declaration */
typedef struct pjsua_media_config pjsua_media_config;


/**
 * Initialize pjsua with the specified settings. All the settings are 
 * optional, and the default values will be used when the config is not
 * specified.
 *
 * Note that #pjsua_create() MUST be called before calling this function.
 *
 * @param ua_cfg	User agent configuration.
 * @param log_cfg	Optional logging configuration.
 * @param media_cfg	Optional media configuration.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_init(const pjsua_config *ua_cfg,
				const pjsua_logging_config *log_cfg,
				const pjsua_media_config *media_cfg);


/**
 * Application is recommended to call this function after all initialization
 * is done, so that the library can do additional checking set up
 * additional 
 *
 * Application may call this function anytime after #pjsua_init().
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_start(void);


/**
 * Destroy pjsua. Application is recommended to perform graceful shutdown
 * before calling this function (such as unregister the account from the SIP 
 * server, terminate presense subscription, and hangup active calls), however,
 * this function will do all of these if it finds there are active sessions
 * that need to be terminated. This function will approximately block for
 * one second to wait for replies from remote.
 *
 * Application.may safely call this function more than once if it doesn't
 * keep track of it's state.
 *
 * @see pjsua_destroy2()
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_destroy(void);


/**
 * Retrieve pjsua state.
 *
 * @return 	pjsua state.
 */
PJ_DECL(pjsua_state) pjsua_get_state(void);


/**
 * Variant of destroy with additional flags.
 *
 * @param flags		Combination of pjsua_destroy_flag enumeration.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_destroy2(unsigned flags);


/**
 * Poll pjsua for events, and if necessary block the caller thread for
 * the specified maximum interval (in miliseconds).
 *
 * Application doesn't normally need to call this function if it has
 * configured worker thread (\a thread_cnt field) in pjsua_config structure,
 * because polling then will be done by these worker threads instead.
 *
 * @param msec_timeout	Maximum time to wait, in miliseconds.
 *
 * @return  The number of events that have been handled during the
 *	    poll. Negative value indicates error, and application
 *	    can retrieve the error as (status = -return_value).
 */
PJ_DECL(int) pjsua_handle_events(unsigned msec_timeout);


/**
 * Signal all worker threads to quit. This will only wait until internal
 * threads are done.
 */
PJ_DECL(void) pjsua_stop_worker_threads(void);


/**
 * Create memory pool to be used by the application. Once application
 * finished using the pool, it must be released with pj_pool_release().
 *
 * @param name		Optional pool name.
 * @param init_size	Initial size of the pool.
 * @param increment	Increment size.
 *
 * @return		The pool, or NULL when there's no memory.
 */
PJ_DECL(pj_pool_t*) pjsua_pool_create(const char *name, pj_size_t init_size,
				      pj_size_t increment);


/**
 * Application can call this function at any time (after pjsua_create(), of
 * course) to change logging settings.
 *
 * @param c		Logging configuration.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_reconfigure_logging(const pjsua_logging_config *c);


/**
 * Internal function to get SIP endpoint instance of pjsua, which is
 * needed for example to register module, create transports, etc.
 * Only valid after #pjsua_init() is called.
 * 
 * @return		SIP endpoint instance.
 */
PJ_DECL(pjsip_endpoint*) pjsua_get_pjsip_endpt(void);

/**
 * Internal function to get media endpoint instance.
 * Only valid after #pjsua_init() is called.
 *
 * @return		Media endpoint instance.
 */
PJ_DECL(pjmedia_endpt*) pjsua_get_pjmedia_endpt(void);

/**
 * Internal function to get PJSUA pool factory.
 * Only valid after #pjsua_create() is called.
 *
 * @return		Pool factory currently used by PJSUA.
 */
PJ_DECL(pj_pool_factory*) pjsua_get_pool_factory(void);



/*****************************************************************************
 * Utilities.
 *
 */

/**
 * This structure is used to represent the result of the STUN server 
 * resolution and testing, the #pjsua_resolve_stun_servers() function.
 * This structure will be passed in #pj_stun_resolve_cb callback.
 */
struct pj_stun_resolve_result
{
    /**
     * Arbitrary data that was passed to #pjsua_resolve_stun_servers()
     * function.
     */
    void	    *token;

    /**
     * This will contain PJ_SUCCESS if at least one usable STUN server
     * is found, otherwise it will contain the last error code during
     * the operation.
     */
    pj_status_t	     status;

    /**
     * The server name that yields successful result. This will only
     * contain value if status is successful.
     */
    pj_str_t	     name;

    /**
     * The server IP address. This will only contain value if status 
     * is successful.
     */
    pj_sockaddr	     addr;

    /**
     * The index of the usable STUN server.
     */
    unsigned	     index;
};


/**
 * This structure describe the parameter passed to #pjsua_handle_ip_change().
 */
typedef struct pjsua_ip_change_param
{
    /**
     * If set to PJ_TRUE, this will restart the transport listener.
     * 
     * Default : PJ_TRUE
     */
    pj_bool_t	    restart_listener;

    /** 
     * If \a restart listener is set to PJ_TRUE, some delay might be needed 
     * for the listener to be restarted. Use this to set the delay.
     * 
     * Default : PJSUA_TRANSPORT_RESTART_DELAY_TIME
     */
    unsigned	    restart_lis_delay;

} pjsua_ip_change_param;


/**
 * This structure describe the account config specific to IP address change.
 */
typedef struct pjsua_ip_change_acc_cfg
{    
    /**
     * Shutdown the transport used for account registration. If this is set to
     * PJ_TRUE, the transport will be shutdown altough it's used by multiple
     * account. Shutdown transport will be followed by re-Registration if
     * pjsua_acc_config.allow_contact_rewrite is enabled.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t		shutdown_tp;

    /**
     * Hangup active calls associated with the account. If this is set to 
     * PJ_TRUE, then the calls will be hang up.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t		hangup_calls;

    /**
     * Specify the call flags used in the re-INVITE when \a hangup_calls is set 
     * to PJ_FALSE. If this is set to 0, no re-INVITE will be sent. The 
     * re-INVITE will be sent after re-Registration is finished.
     *
     * Default: PJSUA_CALL_REINIT_MEDIA | PJSUA_CALL_UPDATE_CONTACT |
     *          PJSUA_CALL_UPDATE_VIA
     */
    unsigned		reinvite_flags;
    
} pjsua_ip_change_acc_cfg;


/**
 * Call this function to initialize \a pjsua_ip_change_param with default 
 * values.
 *
 * @param param	    The IP change param to be initialized.
 */
PJ_DECL(void) pjsua_ip_change_param_default(pjsua_ip_change_param *param);


/**
 * This is a utility function to detect NAT type in front of this
 * endpoint. Once invoked successfully, this function will complete 
 * asynchronously and report the result in \a on_nat_detect() callback
 * of pjsua_callback.
 *
 * After NAT has been detected and the callback is called, application can
 * get the detected NAT type by calling #pjsua_get_nat_type(). Application
 * can also perform NAT detection by calling #pjsua_detect_nat_type()
 * again at later time.
 *
 * Note that STUN must be enabled to run this function successfully.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_detect_nat_type(void);


/**
 * Get the NAT type as detected by #pjsua_detect_nat_type() function.
 * This function will only return useful NAT type after #pjsua_detect_nat_type()
 * has completed successfully and \a on_nat_detect() callback has been called.
 *
 * @param type		NAT type.
 *
 * @return		When detection is in progress, this function will 
 *			return PJ_EPENDING and \a type will be set to 
 *			PJ_STUN_NAT_TYPE_UNKNOWN. After NAT type has been
 *			detected successfully, this function will return
 *			PJ_SUCCESS and \a type will be set to the correct
 *			value. Other return values indicate error and
 *			\a type will be set to PJ_STUN_NAT_TYPE_ERR_UNKNOWN.
 *
 * @see pjsua_call_get_rem_nat_type()
 */
PJ_DECL(pj_status_t) pjsua_get_nat_type(pj_stun_nat_type *type);


/**
 * Update the STUN servers list. The #pjsua_init() must have been called
 * before calling this function.
 *
 * @param count		Number of STUN server entries.
 * @param srv		Array of STUN server entries to try. Please see
 *			the \a stun_srv field in the #pjsua_config 
 *			documentation about the format of this entry.
 * @param wait		Specify non-zero to make the function block until
 *			it gets the result. In this case, the function
 *			will block while the resolution is being done,
 *			and the callback will be called before this function
 *			returns.
 *
 * @return		If \a wait parameter is non-zero, this will return
 *			PJ_SUCCESS if one usable STUN server is found.
 *			Otherwise it will always return PJ_SUCCESS, and
 *			application will be notified about the result in
 *			the callback on_stun_resolution_complete().
 */
PJ_DECL(pj_status_t) pjsua_update_stun_servers(unsigned count, pj_str_t srv[],
					       pj_bool_t wait);


/**
 * Auxiliary function to resolve and contact each of the STUN server
 * entries (sequentially) to find which is usable. The #pjsua_init() must
 * have been called before calling this function.
 *
 * @param count		Number of STUN server entries to try.
 * @param srv		Array of STUN server entries to try. Please see
 *			the \a stun_srv field in the #pjsua_config 
 *			documentation about the format of this entry.
 * @param wait		Specify non-zero to make the function block until
 *			it gets the result. In this case, the function
 *			will block while the resolution is being done,
 *			and the callback will be called before this function
 *			returns.
 * @param token		Arbitrary token to be passed back to application
 *			in the callback.
 * @param cb		Callback to be called to notify the result of
 *			the function.
 *
 * @return		If \a wait parameter is non-zero, this will return
 *			PJ_SUCCESS if one usable STUN server is found.
 *			Otherwise it will always return PJ_SUCCESS, and
 *			application will be notified about the result in
 *			the callback.
 */
PJ_DECL(pj_status_t) pjsua_resolve_stun_servers(unsigned count,
						pj_str_t srv[],
						pj_bool_t wait,
						void *token,
						pj_stun_resolve_cb cb);

/**
 * Cancel pending STUN resolution which match the specified token. 
 *
 * @param token		The token to match. This token was given to 
 *			#pjsua_resolve_stun_servers()
 * @param notify_cb	Boolean to control whether the callback should
 *			be called for cancelled resolutions. When the
 *			callback is called, the status in the result
 *			will be set as PJ_ECANCELLED.
 *
 * @return		PJ_SUCCESS if there is at least one pending STUN
 *			resolution cancelled, or PJ_ENOTFOUND if there is
 *			no matching one, or other error.
 */
PJ_DECL(pj_status_t) pjsua_cancel_stun_resolution(void *token,
						  pj_bool_t notify_cb);


/**
 * This is a utility function to verify that valid SIP url is given. If the
 * URL is a valid SIP/SIPS scheme, PJ_SUCCESS will be returned.
 *
 * @param url		The URL, as NULL terminated string.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 *
 * @see pjsua_verify_url()
 */
PJ_DECL(pj_status_t) pjsua_verify_sip_url(const char *url);


/**
 * This is a utility function to verify that valid URI is given. Unlike
 * pjsua_verify_sip_url(), this function will return PJ_SUCCESS if tel: URI
 * is given.
 *
 * @param url		The URL, as NULL terminated string.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 *
 * @see pjsua_verify_sip_url()
 */
PJ_DECL(pj_status_t) pjsua_verify_url(const char *url);


/**
 * Schedule a timer entry. Note that the timer callback may be executed
 * by different thread, depending on whether worker thread is enabled or
 * not.
 *
 * @param entry		Timer heap entry.
 * @param delay         The interval to expire.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 *
 * @see pjsip_endpt_schedule_timer()
 */
#if PJ_TIMER_DEBUG
#define pjsua_schedule_timer(e,d) pjsua_schedule_timer_dbg(e,d,\
                                                           __FILE__,__LINE__)

PJ_DECL(pj_status_t) pjsua_schedule_timer_dbg(pj_timer_entry *entry,
                                              const pj_time_val *delay,
                                              const char *src_file,
                                              int src_line);
#else
PJ_DECL(pj_status_t) pjsua_schedule_timer(pj_timer_entry *entry,
					  const pj_time_val *delay);
#endif

/**
 * Schedule a callback function to be called after a specified time interval.
 * Note that the callback may be executed by different thread, depending on
 * whether worker thread is enabled or not.
 *
 * @param cb		The callback function.
 * @param user_data     The user data.
 * @param msec_delay    The time interval in msec.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
#if PJ_TIMER_DEBUG
#define pjsua_schedule_timer2(cb,u,d) \
			pjsua_schedule_timer2_dbg(cb,u,d,__FILE__,__LINE__)

PJ_DECL(pj_status_t) pjsua_schedule_timer2_dbg(void (*cb)(void *user_data),
                                               void *user_data,
                                               unsigned msec_delay,
                                               const char *src_file,
                                               int src_line);
#else
PJ_DECL(pj_status_t) pjsua_schedule_timer2(void (*cb)(void *user_data),
                                           void *user_data,
                                           unsigned msec_delay);
#endif

/**
 * Cancel the previously scheduled timer.
 *
 * @param entry		Timer heap entry.
 *
 * @see pjsip_endpt_cancel_timer()
 */
PJ_DECL(void) pjsua_cancel_timer(pj_timer_entry *entry);


/**
 * This is a utility function to display error message for the specified 
 * error code. The error message will be sent to the log.
 *
 * @param sender	The log sender field.
 * @param title		Message title for the error.
 * @param status	Status code.
 */
PJ_DECL(void) pjsua_perror(const char *sender, const char *title, 
			   pj_status_t status);


/**
 * This is a utility function to dump the stack states to log, using
 * verbosity level 3.
 *
 * @param detail	Will print detailed output (such as list of
 *			SIP transactions) when non-zero.
 */
PJ_DECL(void) pjsua_dump(pj_bool_t detail);


/**
 * Inform the stack that IP address change event was detected. 
 * The stack will:
 * 1. Restart the listener (this step is configurable via 
 *    \a pjsua_ip_change_param.restart_listener).
 * 2. Shutdown the transport used by account registration (this step is 
 *    configurable via \a pjsua_acc_config.ip_change_cfg.shutdown_tp).
 * 3. Update contact URI by sending re-Registration (this step is configurable 
 *    via a\ pjsua_acc_config.allow_contact_rewrite and 
 *    a\ pjsua_acc_config.contact_rewrite_method)
 * 4. Hangup active calls (this step is configurable via 
 *    a\ pjsua_acc_config.ip_change_cfg.hangup_calls) or 
 *    continue the call by sending re-INVITE 
 *    (configurable via \a pjsua_acc_config.ip_change_cfg.reinvite_flags).
 *
 * @param param		The IP change parameter, have a look at 
 *			#pjsua_ip_change_param.
 *
 * @return		PJ_SUCCESS on success, other on error.
 */
PJ_DECL(pj_status_t) pjsua_handle_ip_change(
					   const pjsua_ip_change_param *param);


/**
 * @}
 */



/*****************************************************************************
 * TRANSPORT API
 */

/**
 * @defgroup PJSUA_LIB_TRANSPORT PJSUA-API Signaling Transport
 * @ingroup PJSUA_LIB
 * @brief API for managing SIP transports
 * @{
 *
 * PJSUA-API supports creating multiple transport instances, for example UDP,
 * TCP, and TLS transport. SIP transport must be created before adding an 
 * account.
 */


/** SIP transport identification.
 */
typedef int pjsua_transport_id;


/**
 * Transport configuration for creating transports for both SIP
 * and media. Before setting some values to this structure, application
 * MUST call #pjsua_transport_config_default() to initialize its
 * values with default settings.
 */
typedef struct pjsua_transport_config
{
    /**
     * UDP port number to bind locally. This setting MUST be specified
     * even when default port is desired. If the value is zero, the
     * transport will be bound to any available port, and application
     * can query the port by querying the transport info.
     */
    unsigned		port;

    /**
     * Specify the port range for socket binding, relative to the start
     * port number specified in \a port. Note that this setting is only
     * applicable to media transport when the start port number is non zero.
     * Media transport is configurable via account setting, 
     * i.e: pjsua_acc_config.rtp_cfg, please check the media transport 
     * config docs for more info.
     *
     * Available ports are in the range of [\a port, \a port + \a port_range]. 
     * 
     * Default value is zero.
     */
    unsigned		port_range;

    /**
     * Specify whether to randomly pick the starting port number from
     * the range of [\a port, \a port + \a port_range]. This setting is
     * used only if both port and port_range are non-zero, and only
     * applicable for the port selection of UDP and loop media transport.
     * 
     * Default is PJ_FALSE.
     */
    pj_bool_t		randomize_port;

    /**
     * Optional address to advertise as the address of this transport.
     * Application can specify any address or hostname for this field,
     * for example it can point to one of the interface address in the
     * system, or it can point to the public address of a NAT router
     * where port mappings have been configured for the application.
     *
     * Note: this option can be used for both UDP and TCP as well!
     */
    pj_str_t		public_addr;

    /**
     * Optional address where the socket should be bound to. This option
     * SHOULD only be used to selectively bind the socket to particular
     * interface (instead of 0.0.0.0), and SHOULD NOT be used to set the
     * published address of a transport (the public_addr field should be
     * used for that purpose).
     *
     * Note that unlike public_addr field, the address (or hostname) here 
     * MUST correspond to the actual interface address in the host, since
     * this address will be specified as bind() argument.
     */
    pj_str_t		bound_addr;

    /**
     * This specifies TLS settings for TLS transport. It is only be used
     * when this transport config is being used to create a SIP TLS
     * transport.
     */
    pjsip_tls_setting	tls_setting;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default is QoS not set.
     */
    pj_qos_type		qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * Default is QoS not set.
     */
    pj_qos_params	qos_params;

    /**
     * Specify options to be set on the transport. 
     *
     * By default there is no options.
     * 
     */
    pj_sockopt_params	sockopt_params;

} pjsua_transport_config;


/**
 * Call this function to initialize UDP config with default values.
 *
 * @param cfg	    The UDP config to be initialized.
 */
PJ_DECL(void) pjsua_transport_config_default(pjsua_transport_config *cfg);


/**
 * Duplicate transport config.
 *
 * @param pool		The pool.
 * @param dst		The destination config.
 * @param src		The source config.
 */
PJ_DECL(void) pjsua_transport_config_dup(pj_pool_t *pool,
					 pjsua_transport_config *dst,
					 const pjsua_transport_config *src);


/**
 * This structure describes transport information returned by
 * #pjsua_transport_get_info() function.
 */
typedef struct pjsua_transport_info
{
    /**
     * PJSUA transport identification.
     */
    pjsua_transport_id	    id;

    /**
     * Transport type.
     */
    pjsip_transport_type_e  type;

    /**
     * Transport type name.
     */
    pj_str_t		    type_name;

    /**
     * Transport string info/description.
     */
    pj_str_t		    info;

    /**
     * Transport flag (see ##pjsip_transport_flags_e).
     */
    unsigned		    flag;

    /**
     * Local address length.
     */
    unsigned		    addr_len;

    /**
     * Local/bound address.
     */
    pj_sockaddr		    local_addr;

    /**
     * Published address (or transport address name).
     */
    pjsip_host_port	    local_name;

    /**
     * Current number of objects currently referencing this transport.
     */
    unsigned		    usage_count;


} pjsua_transport_info;


/**
 * Create and start a new SIP transport according to the specified
 * settings.
 *
 * @param type		Transport type.
 * @param cfg		Transport configuration.
 * @param p_id		Optional pointer to receive transport ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_create(pjsip_transport_type_e type,
					    const pjsua_transport_config *cfg,
					    pjsua_transport_id *p_id);

/**
 * Register transport that has been created by application. This function
 * is useful if application wants to implement custom SIP transport and use
 * it with pjsua.
 *
 * @param tp		Transport instance.
 * @param p_id		Optional pointer to receive transport ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_register(pjsip_transport *tp,
					      pjsua_transport_id *p_id);


/**
 * Register transport factory that has been created by application.
 * This function is useful if application wants to implement custom SIP
 * transport and use it with pjsua.
 *
 * @param tf		Transport factory instance.
 * @param p_id		Optional pointer to receive transport ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_tpfactory_register( pjsip_tpfactory *tf,
					      pjsua_transport_id *p_id);

/**
 * Enumerate all transports currently created in the system. This function
 * will return all transport IDs, and application may then call 
 * #pjsua_transport_get_info() function to retrieve detailed information
 * about the transport.
 *
 * @param id		Array to receive transport ids.
 * @param count		In input, specifies the maximum number of elements.
 *			On return, it contains the actual number of elements.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_transports( pjsua_transport_id id[],
					    unsigned *count );


/**
 * Get information about transports.
 *
 * @param id		Transport ID.
 * @param info		Pointer to receive transport info.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_get_info(pjsua_transport_id id,
					      pjsua_transport_info *info);


/**
 * Disable a transport or re-enable it. By default transport is always 
 * enabled after it is created. Disabling a transport does not necessarily
 * close the socket, it will only discard incoming messages and prevent
 * the transport from being used to send outgoing messages.
 *
 * @param id		Transport ID.
 * @param enabled	Non-zero to enable, zero to disable.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_set_enable(pjsua_transport_id id,
						pj_bool_t enabled);


/**
 * Close the transport. The system will wait until all transactions are
 * closed while preventing new users from using the transport, and will
 * close the transport when it is safe to do so.
 *
 * NOTE: Forcefully closing transport (force = PJ_TRUE) is deprecated,
 * since any pending transactions that are using the transport may not
 * terminate properly and can even crash. Application wishing to immediately
 * close the transport for the purpose of restarting it should use
 * #pjsua_handle_ip_change() instead.
 *
 * @param id		Transport ID.
 * @param force		Must be PJ_FALSE. force = PJ_TRUE is deprecated.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_close( pjsua_transport_id id,
					    pj_bool_t force );


/**
 * Start the listener of the transport. This is useful when listener is not 
 * automatically started when creating the transport.
 *
 * @param id		Transport ID.
 * @param cfg		The new transport config used by the listener. 
 *			Only port, public_addr and bound_addr are used at the 
 *			moment.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_lis_start( pjsua_transport_id id,
					    const pjsua_transport_config *cfg);


/**
 * @}
 */




/*****************************************************************************
 * ACCOUNT API
 */


/**
 * @defgroup PJSUA_LIB_ACC PJSUA-API Accounts Management
 * @ingroup PJSUA_LIB
 * @brief PJSUA Accounts management
 * @{
 *
 * PJSUA accounts provide identity (or identities) of the user who is currently
 * using the application. In SIP terms, the identity is used as the <b>From</b>
 * header in outgoing requests.
 *
 * PJSUA-API supports creating and managing multiple accounts. The maximum
 * number of accounts is limited by a compile time constant
 * <tt>PJSUA_MAX_ACC</tt>.
 *
 * Account may or may not have client registration associated with it.
 * An account is also associated with <b>route set</b> and some <b>authentication
 * credentials</b>, which are used when sending SIP request messages using the
 * account. An account also has presence's <b>online status</b>, which
 * will be reported to remote peer when they subscribe to the account's
 * presence, or which is published to a presence server if presence 
 * publication is enabled for the account.
 *
 * At least one account MUST be created in the application. If no user
 * association is required, application can create a userless account by
 * calling #pjsua_acc_add_local(). A userless account identifies local endpoint
 * instead of a particular user, and it correspond with a particular
 * transport instance.
 *
 * Also one account must be set as the <b>default account</b>, which is used as
 * the account to use when PJSUA fails to match a request with any other
 * accounts.
 *
 * When sending outgoing SIP requests (such as making calls or sending
 * instant messages), normally PJSUA requires the application to specify
 * which account to use for the request. If no account is specified,
 * PJSUA may be able to select the account by matching the destination
 * domain name, and fall back to default account when no match is found.
 */

/**
 * Maximum accounts.
 */
#ifndef PJSUA_MAX_ACC
#   define PJSUA_MAX_ACC	    8
#endif


/**
 * Default registration interval.
 */
#ifndef PJSUA_REG_INTERVAL
#   define PJSUA_REG_INTERVAL	    300
#endif


/**
 * Default maximum time to wait for account unregistration transactions to
 * complete during library shutdown sequence.
 *
 * Default: 4000 (4 seconds)
 */
#ifndef PJSUA_UNREG_TIMEOUT
#   define PJSUA_UNREG_TIMEOUT	    4000
#endif


/**
 * Default PUBLISH expiration
 */
#ifndef PJSUA_PUBLISH_EXPIRATION
#   define PJSUA_PUBLISH_EXPIRATION PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED
#endif


/**
 * Default account priority.
 */
#ifndef PJSUA_DEFAULT_ACC_PRIORITY
#   define PJSUA_DEFAULT_ACC_PRIORITY	0
#endif


/**
 * Maximum time to wait for unpublication transaction(s) to complete
 * during shutdown process, before sending unregistration. The library
 * tries to wait for the unpublication (un-PUBLISH) to complete before
 * sending REGISTER request to unregister the account, during library
 * shutdown process. If the value is set too short, it is possible that
 * the unregistration is sent before unpublication completes, causing
 * unpublication request to fail.
 *
 * Default: 2000 (2 seconds)
 */
#ifndef PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC
#   define PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC	2000
#endif


/**
 * Default auto retry re-registration interval, in seconds. Set to 0
 * to disable this. Application can set the timer on per account basis 
 * by setting the pjsua_acc_config.reg_retry_interval field instead.
 *
 * Default: 300 (5 minutes)
 */
#ifndef PJSUA_REG_RETRY_INTERVAL
#   define PJSUA_REG_RETRY_INTERVAL	300
#endif

/**
 * When the registration is successfull, the auto registration refresh will
 * be sent before it expires. Setting this to 0 will disable it.
 * This is useful for app that uses Push Notification and doesn't require auto
 * registration refresh. App can periodically send refresh registration or
 * send it before making a call.=
 * See https://github.com/pjsip/pjproject/pull/2652 for more info.
 *
 * Default: 1 (enabled)
 */
#ifndef PJSUA_REG_AUTO_REG_REFRESH
#   define PJSUA_REG_AUTO_REG_REFRESH     1
#endif

/**
 * This macro specifies the default value for \a contact_rewrite_method
 * field in pjsua_acc_config. It specifies how Contact update will be
 * done with the registration, if \a allow_contact_rewrite is enabled in
 * the account config. See \a pjsua_contact_rewrite_method for the options.
 *
 * Value PJSUA_CONTACT_REWRITE_UNREGISTER(1) is the legacy behavior.
 *
 * Default value: PJSUA_CONTACT_REWRITE_NO_UNREG(2) |
 *                PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE(4)
 */
#ifndef PJSUA_CONTACT_REWRITE_METHOD
#   define PJSUA_CONTACT_REWRITE_METHOD	   (PJSUA_CONTACT_REWRITE_NO_UNREG | \
                                           PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE)
#endif


/**
 * Bit value used in pjsua_acc_config.reg_use_proxy field to indicate that
 * the global outbound proxy list should be added to the REGISTER request.
 */
#define PJSUA_REG_USE_OUTBOUND_PROXY		1


/**
 * Bit value used in pjsua_acc_config.reg_use_proxy field to indicate that
 * the account proxy list should be added to the REGISTER request.
 */
#define PJSUA_REG_USE_ACC_PROXY			2


/**
 * This enumeration specifies how we should offer call hold request to
 * remote peer. The default value is set by compile time constant
 * PJSUA_CALL_HOLD_TYPE_DEFAULT, and application may control the setting
 * on per-account basis by manipulating \a call_hold_type field in
 * #pjsua_acc_config.
 */
typedef enum pjsua_call_hold_type
{
    /**
     * This will follow RFC 3264 recommendation to use a=sendonly,
     * a=recvonly, and a=inactive attribute as means to signal call
     * hold status. This is the correct value to use.
     */
    PJSUA_CALL_HOLD_TYPE_RFC3264,

    /**
     * This will use the old and deprecated method as specified in RFC 2543,
     * and will offer c=0.0.0.0 in the SDP instead. Using this has many
     * drawbacks such as inability to keep the media transport alive while
     * the call is being put on hold, and should only be used if remote
     * does not understand RFC 3264 style call hold offer.
     */
    PJSUA_CALL_HOLD_TYPE_RFC2543

} pjsua_call_hold_type;


/**
 * Specify the default call hold type to be used in #pjsua_acc_config.
 *
 * Default is PJSUA_CALL_HOLD_TYPE_RFC3264, and there's no reason to change
 * this except if you're communicating with an old/non-standard peer.
 */
#ifndef PJSUA_CALL_HOLD_TYPE_DEFAULT
#   define PJSUA_CALL_HOLD_TYPE_DEFAULT		PJSUA_CALL_HOLD_TYPE_RFC3264
#endif

/**
 * This enumeration controls the use of STUN in the account.
 */
typedef enum pjsua_stun_use
{
    /**
     * Follow the default setting in the global \a pjsua_config.
     */
    PJSUA_STUN_USE_DEFAULT,

    /**
     * Disable STUN. If STUN is not enabled in the global \a pjsua_config,
     * this setting has no effect.
     */
    PJSUA_STUN_USE_DISABLED,
    
    /**
     * Retry other STUN servers if the STUN server selected during
     * startup (#pjsua_init()) or after calling #pjsua_update_stun_servers()
     * is unavailable during runtime. This setting is valid only for
     * account's media STUN setting and if the call is using UDP media
     * transport.
     */
    PJSUA_STUN_RETRY_ON_FAILURE

} pjsua_stun_use;

/**
 * This enumeration controls the use of UPnP in the account.
 */
typedef enum pjsua_upnp_use
{
    /**
     * Follow the default setting in the global \a pjsua_config.
     */
    PJSUA_UPNP_USE_DEFAULT,

    /**
     * Disable UPnP.
     */
    PJSUA_UPNP_USE_DISABLED

} pjsua_upnp_use;

/**
 * This enumeration controls the use of ICE settings in the account.
 */
typedef enum pjsua_ice_config_use
{
    /**
     * Use the default settings in the global \a pjsua_media_config.
     */
    PJSUA_ICE_CONFIG_USE_DEFAULT,

    /**
     * Use the custom \a pjsua_ice_config setting in the account.
     */
    PJSUA_ICE_CONFIG_USE_CUSTOM

} pjsua_ice_config_use;

/**
 * This enumeration controls the use of TURN settings in the account.
 */
typedef enum pjsua_turn_config_use
{
    /**
     * Use the default setting in the global \a pjsua_media_config.
     */
    PJSUA_TURN_CONFIG_USE_DEFAULT,

    /**
     * Use the custom \a pjsua_turn_config setting in the account.
     */
    PJSUA_TURN_CONFIG_USE_CUSTOM

} pjsua_turn_config_use;

/**
 * ICE setting. This setting is used in the pjsua_acc_config.
 */
typedef struct pjsua_ice_config
{
    /**
     * Enable ICE.
     */
    pj_bool_t		enable_ice;

    /**
     * Set the maximum number of host candidates.
     *
     * Default: -1 (maximum not set)
     */
    int			ice_max_host_cands;

    /**
     * ICE session options.
     */
    pj_ice_sess_options	ice_opt;

    /**
     * Disable RTCP component.
     *
     * Default: no
     */
    pj_bool_t		ice_no_rtcp;

    /**
     * Send re-INVITE/UPDATE every after ICE connectivity check regardless
     * the default ICE transport address is changed or not. When this is set
     * to PJ_FALSE, re-INVITE/UPDATE will be sent only when the default ICE
     * transport address is changed.
     *
     * Default: yes
     */
    pj_bool_t		ice_always_update;

} pjsua_ice_config;

/**
 * TURN setting. This setting is used in the pjsua_acc_config.
 */
typedef struct pjsua_turn_config
{
    /**
     * Enable TURN candidate in ICE.
     */
    pj_bool_t		enable_turn;

    /**
     * Specify TURN domain name or host name, in in "DOMAIN:PORT" or
     * "HOST:PORT" format.
     */
    pj_str_t		turn_server;

    /**
     * Specify the connection type to be used to the TURN server. Valid
     * values are PJ_TURN_TP_UDP, PJ_TURN_TP_TCP or PJ_TURN_TP_TLS.
     *
     * Default: PJ_TURN_TP_UDP
     */
    pj_turn_tp_type	turn_conn_type;

    /**
     * Specify the credential to authenticate with the TURN server.
     */
    pj_stun_auth_cred	turn_auth_cred;

    /**
     * This specifies TLS settings for TURN TLS. It is only be used
     * when this TLS is used to connect to the TURN server.
     */
    pj_turn_sock_tls_cfg turn_tls_setting;

} pjsua_turn_config;

/**
 * Specify how IPv6 transport should be used in account config.
 */
typedef enum pjsua_ipv6_use
{
    /**
     * IPv6 is not used.
     */
    PJSUA_IPV6_DISABLED,

    /**
     * IPv6 is enabled.
     */
    PJSUA_IPV6_ENABLED

} pjsua_ipv6_use;

/**
 * Specify NAT64 options to be used in account config.
 */
typedef enum pjsua_nat64_opt
{
    /**
     * NAT64 is not used.
     */
    PJSUA_NAT64_DISABLED,

    /**
     * NAT64 is enabled.
     */
    PJSUA_NAT64_ENABLED
    
} pjsua_nat64_opt;


/**
 * This structure describes account configuration to be specified when
 * adding a new account with #pjsua_acc_add(). Application MUST initialize
 * this structure first by calling #pjsua_acc_config_default().
 */
typedef struct pjsua_acc_config
{
    /**
     * Arbitrary user data to be associated with the newly created account.
     * Application may set this later with #pjsua_acc_set_user_data() and
     * retrieve it with #pjsua_acc_get_user_data().
     */
    void	   *user_data;

    /**
     * Account priority, which is used to control the order of matching
     * incoming/outgoing requests. The higher the number means the higher
     * the priority is, and the account will be matched first.
     */
    int		    priority;

    /** 
     * The full SIP URL for the account. The value can take name address or 
     * URL format, and will look something like "sip:account@serviceprovider"
     * or "\"Display Name\" <sip:account@provider>".
     *
     * This field is mandatory.
     */
    pj_str_t	    id;

    /** 
     * This is the URL to be put in the request URI for the registration,
     * and will look something like "sip:serviceprovider".
     *
     * This field should be specified if registration is desired. If the
     * value is empty, no account registration will be performed.
     */
    pj_str_t	    reg_uri;

    /** 
     * The optional custom SIP headers to be put in the registration
     * request.
     */
    pjsip_hdr	    reg_hdr_list;

    /**
     * Additional parameters that will be appended in the Contact header
     * for this account. This will only affect REGISTER requests and
     * will be appended after \a contact_params;
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    pj_str_t	    reg_contact_params;

    /**
     * Additional URI parameters that will be appended in the Contact URI
     * for this account. This will only affect REGISTER requests and
     * will be appended after \a contact_uri_params;
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    pj_str_t	    reg_contact_uri_params;

    /** 
     * The optional custom SIP headers to be put in the presence
     * subscription request.
     */
    pjsip_hdr	    sub_hdr_list;

    /**
     * Subscribe to message waiting indication events (RFC 3842).
     *
     * See also \a enable_unsolicited_mwi field on #pjsua_config.
     *
     * Default: no
     */
    pj_bool_t	    mwi_enabled;

    /**
     * Specify the default expiration time for Message Waiting Indication
     * (RFC 3842) event subscription. This must not be zero.
     *
     * Default: PJSIP_MWI_DEFAULT_EXPIRES
     */
    unsigned	    mwi_expires;

    /**
     * If this flag is set, the presence information of this account will
     * be PUBLISH-ed to the server where the account belongs.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t	    publish_enabled;

    /**
     * Event publication options.
     */
    pjsip_publishc_opt	publish_opt;

    /**
     * Maximum time to wait for unpublication transaction(s) to complete
     * during shutdown process, before sending unregistration. The library
     * tries to wait for the unpublication (un-PUBLISH) to complete before
     * sending REGISTER request to unregister the account, during library
     * shutdown process. If the value is set too short, it is possible that
     * the unregistration is sent before unpublication completes, causing
     * unpublication request to fail.
     *
     * Default: PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC
     */
    unsigned	    unpublish_max_wait_time_msec;

    /**
     * Authentication preference.
     */
    pjsip_auth_clt_pref auth_pref;

    /**
     * Optional PIDF tuple ID for outgoing PUBLISH and NOTIFY. If this value
     * is not specified, a random string will be used.
     */
    pj_str_t	    pidf_tuple_id;

    /** 
     * Optional URI to be put as Contact for this account. It is recommended
     * that this field is left empty, so that the value will be calculated
     * automatically based on the transport address.
     */
    pj_str_t	    force_contact;

    /**
     * Additional parameters that will be appended in the Contact header
     * for this account. This will affect the Contact header in all SIP 
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    pj_str_t	    contact_params;

    /**
     * Additional URI parameters that will be appended in the Contact URI
     * for this account. This will affect the Contact URI in all SIP
     * messages sent on behalf of this account, including but not limited to
     * REGISTER, INVITE, and SUBCRIBE requests or responses.
     *
     * The parameters should be preceeded by semicolon, and all strings must
     * be properly escaped. Example:
     *	 ";my-param=X;another-param=Hi%20there"
     */
    pj_str_t	    contact_uri_params;

    /**
     * Specify how support for reliable provisional response (100rel/
     * PRACK) should be used for all sessions in this account. See the
     * documentation of pjsua_100rel_use enumeration for more info.
     *
     * Default: The default value is taken from the value of
     *          require_100rel in pjsua_config.
     */
    pjsua_100rel_use require_100rel;

    /**
     * Specify the usage of Session Timers for all sessions. See the
     * #pjsua_sip_timer_use for possible values.
     *
     * Default: PJSUA_SIP_TIMER_OPTIONAL
     */
    pjsua_sip_timer_use use_timer;

    /**
     * Specify Session Timer settings, see #pjsip_timer_setting. 
     */
    pjsip_timer_setting timer_setting;

    /**
     * Number of proxies in the proxy array below.
     */
    unsigned	    proxy_cnt;

    /** 
     * Optional URI of the proxies to be visited for all outgoing requests 
     * that are using this account (REGISTER, INVITE, etc). Application need 
     * to specify these proxies if the service provider requires that requests
     * destined towards its network should go through certain proxies first
     * (for example, border controllers).
     *
     * These proxies will be put in the route set for this account, with 
     * maintaining the orders (the first proxy in the array will be visited
     * first). If global outbound proxies are configured in pjsua_config,
     * then these account proxies will be placed after the global outbound
     * proxies in the routeset.
     */
    pj_str_t	    proxy[PJSUA_ACC_MAX_PROXIES];

    /**
     * If remote sends SDP answer containing more than one format or codec in
     * the media line, send re-INVITE or UPDATE with just one codec to lock
     * which codec to use.
     *
     * Default: 1 (Yes). Set to zero to disable.
     */
    unsigned	    lock_codec;

    /** 
     * Optional interval for registration, in seconds. If the value is zero, 
     * default interval will be used (PJSUA_REG_INTERVAL, 300 seconds).
     */
    unsigned	    reg_timeout;

    /**
     * Specify the number of seconds to refresh the client registration
     * before the registration expires.
     *
     * Default: PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH, 5 seconds
     */
    unsigned	    reg_delay_before_refresh;

    /**
     * Specify the maximum time to wait for unregistration requests to
     * complete during library shutdown sequence.
     *
     * Default: PJSUA_UNREG_TIMEOUT
     */
    unsigned	    unreg_timeout;

    /** 
     * Number of credentials in the credential array.
     */
    unsigned	    cred_count;

    /** 
     * Array of credentials. If registration is desired, normally there should
     * be at least one credential specified, to successfully authenticate
     * against the service provider. More credentials can be specified, for
     * example when the requests are expected to be challenged by the
     * proxies in the route set.
     */
    pjsip_cred_info cred_info[PJSUA_ACC_MAX_PROXIES];

    /**
     * Optionally bind this account to specific transport. This normally is
     * not a good idea, as account should be able to send requests using
     * any available transports according to the destination. But some
     * application may want to have explicit control over the transport to
     * use, so in that case it can set this field.
     *
     * Default: -1 (PJSUA_INVALID_ID)
     *
     * @see pjsua_acc_set_transport()
     */
    pjsua_transport_id  transport_id;

    /**
     * This option is used to update the transport address and the Contact
     * header of REGISTER request. When this option is  enabled, the library 
     * will keep track of the public IP address from the response of REGISTER
     * request. Once it detects that the address has changed, it will 
     * unregister current Contact, update the Contact with transport address
     * learned from Via header, and register a new Contact to the registrar.
     * This will also update the public name of UDP transport if STUN is
     * configured. 
     *
     * See also contact_rewrite_method field.
     *
     * Default: 1 (yes)
     */
    pj_bool_t allow_contact_rewrite;

    /**
     * Specify how Contact update will be done with the registration, if
     * \a allow_contact_rewrite is enabled. The value is bitmask combination of
     * \a pjsua_contact_rewrite_method. See also pjsua_contact_rewrite_method.
     *
     * Value PJSUA_CONTACT_REWRITE_UNREGISTER(1) is the legacy behavior.
     *
     * Default value: PJSUA_CONTACT_REWRITE_METHOD
     * (PJSUA_CONTACT_REWRITE_NO_UNREG | PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE)
     */
    int		     contact_rewrite_method;

    /**
     * Specify if source TCP port should be used as the initial Contact
     * address if TCP/TLS transport is used. Note that this feature will
     * be automatically turned off when nameserver is configured because
     * it may yield different destination address due to DNS SRV resolution.
     * Also some platforms are unable to report the local address of the
     * TCP socket when it is still connecting. In these cases, this
     * feature will also be turned off.
     *
     * Default: PJ_TRUE (yes).
     */
    pj_bool_t	     contact_use_src_port;

    /**
     * This option is used to overwrite the "sent-by" field of the Via header
     * for outgoing messages with the same interface address as the one in
     * the REGISTER request, as long as the request uses the same transport
     * instance as the previous REGISTER request.
     *
     * Default: 1 (yes)
     */
    pj_bool_t        allow_via_rewrite;

    /**
     * This option controls whether the IP address in SDP should be replaced
     * with the IP address found in Via header of the REGISTER response, ONLY
     * when STUN and ICE are not used. If the value is FALSE (the original
     * behavior), then the local IP address will be used. If TRUE, and when
     * STUN and ICE are disabled, then the IP address found in registration
     * response will be used.
     *
     * Default: PJ_FALSE (no)
     */
    pj_bool_t        allow_sdp_nat_rewrite;

    /**
     * Control the use of SIP outbound feature. SIP outbound is described in
     * RFC 5626 to enable proxies or registrar to send inbound requests back
     * to UA using the same connection initiated by the UA for its
     * registration. This feature is highly useful in NAT-ed deployemtns,
     * hence it is enabled by default.
     *
     * Note: currently SIP outbound can only be used with TCP and TLS
     * transports. If UDP is used for the registration, the SIP outbound
     * feature will be silently ignored for the account.
     *
     * Default: PJ_TRUE
     */
    unsigned	     use_rfc5626;

    /**
     * Specify SIP outbound (RFC 5626) instance ID to be used by this
     * application. If empty, an instance ID will be generated based on
     * the hostname of this agent. If application specifies this parameter, the
     * value will look like "<urn:uuid:00000000-0000-1000-8000-AABBCCDDEEFF>"
     * without the doublequote.
     *
     * Default: empty
     */
    pj_str_t	     rfc5626_instance_id;

    /**
     * Specify SIP outbound (RFC 5626) registration ID. The default value
     * is empty, which would cause the library to automatically generate
     * a suitable value.
     *
     * Default: empty
     */
    pj_str_t	     rfc5626_reg_id;

    /**
     * Set the interval for periodic keep-alive transmission for this account.
     * If this value is zero, keep-alive will be disabled for this account.
     * The keep-alive transmission will be sent to the registrar's address,
     * after successful registration.
     *
     * Default: 15 (seconds)
     */
    unsigned	     ka_interval;

    /**
     * Specify the data to be transmitted as keep-alive packets.
     *
     * Default: CR-LF
     */
    pj_str_t	     ka_data;

    /**
     * Specify whether incoming video should be shown to screen by default.
     * This applies to incoming call (INVITE), incoming re-INVITE, and
     * incoming UPDATE requests.
     *
     * Regardless of this setting, application can detect incoming video
     * by implementing \a on_call_media_state() callback and enumerating
     * the media stream(s) with #pjsua_call_get_info(). Once incoming
     * video is recognised, application may retrieve the window associated
     * with the incoming video and show or hide it with
     * #pjsua_vid_win_set_show().
     *
     * Default: PJ_FALSE
     */
    pj_bool_t        vid_in_auto_show;

    /**
     * Specify whether outgoing video should be activated by default when
     * making outgoing calls and/or when incoming video is detected. This
     * applies to incoming and outgoing calls, incoming re-INVITE, and
     * incoming UPDATE. If the setting is non-zero, outgoing video
     * transmission will be started as soon as response to these requests
     * is sent (or received).
     *
     * Regardless of the value of this setting, application can start and
     * stop outgoing video transmission with #pjsua_call_set_vid_strm().
     *
     * Default: PJ_FALSE
     */
    pj_bool_t        vid_out_auto_transmit;

    /**
     * Specify video window's flags. The value is a bitmask combination of
     * #pjmedia_vid_dev_wnd_flag.
     *
     * Default: 0
     */
    unsigned         vid_wnd_flags;

    /**
     * Specify the default capture device to be used by this account. If
     * \a vid_out_auto_transmit is enabled, this device will be used for
     * capturing video.
     *
     * Default: PJMEDIA_VID_DEFAULT_CAPTURE_DEV
     */
    pjmedia_vid_dev_index vid_cap_dev;

    /**
     * Specify the default rendering device to be used by this account.
     *
     * Default: PJMEDIA_VID_DEFAULT_RENDER_DEV
     */
    pjmedia_vid_dev_index vid_rend_dev;

    /**
     * Specify the send rate control for video stream.
     *
     * Default: see #pjmedia_vid_stream_rc_config
     */
    pjmedia_vid_stream_rc_config vid_stream_rc_cfg;

    /**
     * Specify the send keyframe config for video stream.
     *
     * Default: see #pjmedia_vid_stream_sk_config
     */
    pjmedia_vid_stream_sk_config vid_stream_sk_cfg;

    /**
     * Media transport config.
     * 
     * For \a port and \a port_range settings, RTCP port is selected as 
     * RTP port+1.
     * Example: \a port=5000, \a port_range=4
     * - Available ports: 5000, 5002, 5004 (Media/RTP transport)
     *                    5001, 5003, 5005 (Media/RTCP transport)
     */
    pjsua_transport_config rtp_cfg;

    /**
     * Specify NAT64 options.
     *
     * Default: PJSUA_NAT64_DISABLED
     */
    pjsua_nat64_opt 		nat64_opt;

    /**
     * Specify whether IPv6 should be used on media.
     */
    pjsua_ipv6_use     		ipv6_media_use;

    /**
     * Control the use of STUN for the SIP signaling.
     *
     * Default: PJSUA_STUN_USE_DEFAULT
     */
    pjsua_stun_use 		sip_stun_use;

    /**
     * Control the use of STUN for the media transports.
     *
     * Default: PJSUA_STUN_RETRY_ON_FAILURE
     */
    pjsua_stun_use 		media_stun_use;

    /**
     * Control the use of UPnP for the SIP signaling.
     *
     * Default: PJSUA_UPNP_USE_DEFAULT
     */
    pjsua_upnp_use 		sip_upnp_use;

    /**
     * Control the use of UPnP for the media transports.
     *
     * Default: PJSUA_UPNP_USE_DEFAULT
     */
    pjsua_upnp_use 		media_upnp_use;

    /**
     * Use loopback media transport. This may be useful if application
     * doesn't want PJSIP to create real media transports/sockets, such as
     * when using third party media.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t			use_loop_med_tp;

    /**
     * Enable local loopback when loop_med_tp_use is set to PJ_TRUE.
     * If enabled, packets sent to the transport will be sent back to
     * the streams attached to the transport.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t			enable_loopback;

    /**
     * Control the use of ICE in the account. By default, the settings in the
     * \a pjsua_media_config will be used.
     *
     * Default: PJSUA_ICE_CONFIG_USE_DEFAULT
     */
    pjsua_ice_config_use	ice_cfg_use;

    /**
     * The custom ICE setting for this account. This setting will only be
     * used if \a ice_cfg_use is set to PJSUA_ICE_CONFIG_USE_CUSTOM
     */
    pjsua_ice_config		ice_cfg;

    /**
     * Control the use of TURN in the account. By default, the settings in the
     * \a pjsua_media_config will be used
     *
     * Default: PJSUA_TURN_CONFIG_USE_DEFAULT
     */
    pjsua_turn_config_use	turn_cfg_use;

    /**
     * The custom TURN setting for this account. This setting will only be
     * used if \a turn_cfg_use is set to PJSUA_TURN_CONFIG_USE_CUSTOM
     */
    pjsua_turn_config		turn_cfg;

    /**
     * Specify whether secure media transport should be used for this account.
     * Valid values are PJMEDIA_SRTP_DISABLED, PJMEDIA_SRTP_OPTIONAL, and
     * PJMEDIA_SRTP_MANDATORY.
     *
     * Default: #PJSUA_DEFAULT_USE_SRTP
     */
    pjmedia_srtp_use		use_srtp;

    /**
     * Specify whether SRTP requires secure signaling to be used. This option
     * is only used when \a use_srtp option above is non-zero.
     *
     * Valid values are:
     *	0: SRTP does not require secure signaling
     *	1: SRTP requires secure transport such as TLS
     *	2: SRTP requires secure end-to-end transport (SIPS)
     *
     * Default: #PJSUA_DEFAULT_SRTP_SECURE_SIGNALING
     */
    int		     srtp_secure_signaling;

    /**
     * This setting has been deprecated and will be ignored.
     */
    pj_bool_t	     srtp_optional_dup_offer;

    /**
     * Specify SRTP transport setting. Application can initialize it with
     * default values using pjsua_srtp_opt_default().
     */
    pjsua_srtp_opt   srtp_opt;

    /**
     * Specify interval of auto registration retry upon registration failure,
     * in seconds. Set to 0 to disable auto re-registration. Note that
     * registration will only be automatically retried for temporal failures
     * considered to be recoverable in relatively short term, such as:
     * 408 (Request Timeout), 480 (Temporarily Unavailable),
     * 500 (Internal Server Error), 502 (Bad Gateway),
     * 503 (Service Unavailable), 504 (Server Timeout),
     * 6xx (global failure), and failure caused by transport problem.
     * For registration retry caused by transport failure, the first retry
     * will be done after \a reg_first_retry_interval seconds instead.
     * Note that the interval will be randomized slightly by some seconds
     * (specified in \a reg_retry_random_interval) to avoid all clients
     * re-registering at the same time.
     *
     * See also \a reg_first_retry_interval setting.
     *
     * Default: #PJSUA_REG_RETRY_INTERVAL
     */
    unsigned	     reg_retry_interval;

    /**
     * This specifies the interval for the first registration retry. The
     * registration retry is explained in \a reg_retry_interval. Note that
     * the value here will also be randomized by some seconds (specified
     * in \a reg_retry_random_interval) to avoid all clients re-registering
     * at the same time.
     *
     * Default: 0
     */
    unsigned	     reg_first_retry_interval;

    /**
     * This specifies maximum randomized value to be added/substracted
     * to/from the registration retry interval specified in \a
     * reg_retry_interval and \a reg_first_retry_interval, in second.
     * This is useful to avoid all clients re-registering at the same time.
     * For example, if the registration retry interval is set to 100 seconds
     * and this is set to 10 seconds, the actual registration retry interval
     * will be in the range of 90 to 110 seconds.
     *
     * Default: 10
     */
    unsigned	     reg_retry_random_interval;

    /**
     * Specify whether calls of the configured account should be dropped
     * after registration failure and an attempt of re-registration has 
     * also failed.
     *
     * Default: PJ_FALSE (disabled)
     */
    pj_bool_t	     drop_calls_on_reg_fail;

    /**
     * Specify how the registration uses the outbound and account proxy
     * settings. This controls if and what Route headers will appear in
     * the REGISTER request of this account. The value is bitmask combination
     * of PJSUA_REG_USE_OUTBOUND_PROXY and PJSUA_REG_USE_ACC_PROXY bits.
     * If the value is set to 0, the REGISTER request will not use any proxy
     * (i.e. it will not have any Route headers).
     *
     * Default: 3 (PJSUA_REG_USE_OUTBOUND_PROXY | PJSUA_REG_USE_ACC_PROXY)
     */
    unsigned	     reg_use_proxy;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
    /**
     * Specify whether stream keep-alive and NAT hole punching with
     * non-codec-VAD mechanism (see @ref PJMEDIA_STREAM_ENABLE_KA) is enabled
     * for this account.
     *
     * Default: PJ_FALSE (disabled)
     */
    pj_bool_t	     use_stream_ka;

    /**
     * Specify the keepalive configuration for stream.
     *
     * Default: see #pjmedia_stream_ka_config
     */
    pjmedia_stream_ka_config stream_ka_cfg;
#endif

    /**
     * Specify how to offer call hold to remote peer. Please see the
     * documentation on #pjsua_call_hold_type for more info.
     *
     * Default: PJSUA_CALL_HOLD_TYPE_DEFAULT
     */
    pjsua_call_hold_type call_hold_type;
    
    
    /**
     * Specify whether the account should register as soon as it is
     * added to the UA. Application can set this to PJ_FALSE and control
     * the registration manually with pjsua_acc_set_registration().
     *
     * Default: PJ_TRUE
     */
    pj_bool_t		register_on_acc_add;

    /**
     * Specify account configuration specific to IP address change used when
     * calling #pjsua_handle_ip_change().
     */
    pjsua_ip_change_acc_cfg ip_change_cfg;

    /**
     * Enable RTP and RTCP multiplexing.
     */
    pj_bool_t		enable_rtcp_mux;

    /**
     * RTCP Feedback configuration.
     */
    pjmedia_rtcp_fb_setting rtcp_fb_cfg;

    /**
     * Enable RTCP Extended Report (RTCP XR).
     *
     * Default: PJMEDIA_STREAM_ENABLE_XR
     */
    pj_bool_t		enable_rtcp_xr;

} pjsua_acc_config;


/**
 * Initialize ICE config from a media config. If the \a pool argument
 * is NULL, a simple memcpy() will be used.
 *
 * @param pool	    Memory to duplicate strings.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_ice_config_from_media_config(pj_pool_t *pool,
                                              pjsua_ice_config *dst,
                                              const pjsua_media_config *src);

/**
 * Clone. If the \a pool argument is NULL, a simple memcpy() will be used.
 *
 * @param pool	    Memory to duplicate strings.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_ice_config_dup( pj_pool_t *pool,
                                    pjsua_ice_config *dst,
                                    const pjsua_ice_config *src);

/**
 * Initialize TURN config from a media config. If the \a pool argument
 * is NULL, a simple memcpy() will be used.
 *
 * @param pool	    Memory to duplicate strings.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_turn_config_from_media_config(pj_pool_t *pool,
                                               pjsua_turn_config *dst,
                                               const pjsua_media_config *src);

/**
 * Clone. If the \a pool argument is NULL, a simple memcpy() will be used.
 *
 * @param pool	    Memory to duplicate strings.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_DECL(void) pjsua_turn_config_dup(pj_pool_t *pool,
                                    pjsua_turn_config *dst,
                                    const pjsua_turn_config *src);


/**
 * Call this function to initialize SRTP config with default values.
 *
 * @param cfg	    The SRTP config to be initialized.
 */
PJ_DECL(void) pjsua_srtp_opt_default(pjsua_srtp_opt *cfg);


/**
 * Duplicate SRTP transport setting. If the \a pool argument is NULL,
 * a simple memcpy() will be used.
 *
 * @param pool	    Memory to duplicate strings.
 * @param dst	    Destination setting.
 * @param src	    Source setting.
 * @param check_str If set to TRUE, the function will check if strings
 *		    are identical before copying. Identical strings
 *		    will not be duplicated.
 *		    If set to FALSE, all strings will be duplicated.
 */
PJ_DECL(void) pjsua_srtp_opt_dup(pj_pool_t *pool, pjsua_srtp_opt *dst,
                                 const pjsua_srtp_opt *src,
                                 pj_bool_t check_str);


/**
 * Call this function to initialize account config with default values.
 *
 * @param cfg	    The account config to be initialized.
 */
PJ_DECL(void) pjsua_acc_config_default(pjsua_acc_config *cfg);


/**
 * Duplicate account config.
 *
 * @param pool	    Pool to be used for duplicating the config.
 * @param dst	    Destination configuration.
 * @param src	    Source configuration.
 */
PJ_DECL(void) pjsua_acc_config_dup(pj_pool_t *pool,
				   pjsua_acc_config *dst,
				   const pjsua_acc_config *src);


/**
 * Account info. Application can query account info by calling 
 * #pjsua_acc_get_info().
 */
typedef struct pjsua_acc_info
{
    /** 
     * The account ID. 
     */
    pjsua_acc_id	id;

    /**
     * Flag to indicate whether this is the default account.
     */
    pj_bool_t		is_default;

    /** 
     * Account URI 
     */
    pj_str_t		acc_uri;

    /** 
     * Flag to tell whether this account has registration setting
     * (reg_uri is not empty).
     */
    pj_bool_t		has_registration;

    /**
     * An up to date expiration interval for account registration session,
     * PJSIP_EXPIRES_NOT_SPECIFIED if the account doesn't have reg session.
     */
    unsigned		expires;

    /**
     * Last registration status code. If status code is zero, the account
     * is currently not registered. Any other value indicates the SIP
     * status code of the registration.
     */
    pjsip_status_code	status;

    /**
     * Last registration error code. When the status field contains a SIP
     * status code that indicates a registration failure, last registration
     * error code contains the error code that causes the failure. In any
     * other case, its value is zero.
     */
    pj_status_t	        reg_last_err;

    /**
     * String describing the registration status.
     */
    pj_str_t		status_text;

    /**
     * Presence online status for this account.
     */
    pj_bool_t		online_status;

    /**
     * Presence online status text.
     */
    pj_str_t		online_status_text;

    /**
     * Extended RPID online status information.
     */
    pjrpid_element	rpid;

    /**
     * Buffer that is used internally to store the status text.
     */
    char		buf_[PJ_ERR_MSG_SIZE];

} pjsua_acc_info;



/**
 * Get number of current accounts.
 *
 * @return		Current number of accounts.
 */
PJ_DECL(unsigned) pjsua_acc_get_count(void);


/**
 * Check if the specified account ID is valid.
 *
 * @param acc_id	Account ID to check.
 *
 * @return		Non-zero if account ID is valid.
 */
PJ_DECL(pj_bool_t) pjsua_acc_is_valid(pjsua_acc_id acc_id);


/**
 * Set default account to be used when incoming and outgoing
 * requests doesn't match any accounts.
 *
 * @param acc_id	The account ID to be used as default.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_default(pjsua_acc_id acc_id);


/**
 * Get default account to be used when receiving incoming requests (calls),
 * when the destination of the incoming call doesn't match any other
 * accounts.
 *
 * @return		The default account ID, or PJSUA_INVALID_ID if no
 *			default account is configured.
 */
PJ_DECL(pjsua_acc_id) pjsua_acc_get_default(void);


/**
 * Add a new account to pjsua. PJSUA must have been initialized (with
 * #pjsua_init()) before calling this function. If registration is configured
 * for this account, this function would also start the SIP registration
 * session with the SIP registrar server. This SIP registration session
 * will be maintained internally by the library, and application doesn't
 * need to do anything to maintain the registration session.
 *
 *
 * @param acc_cfg	Account configuration.
 * @param is_default	If non-zero, this account will be set as the default
 *			account. The default account will be used when sending
 *			outgoing requests (e.g. making call) when no account is
 *			specified, and when receiving incoming requests when the
 *			request does not match any accounts. It is recommended
 *			that default account is set to local/LAN account.
 * @param p_acc_id	Pointer to receive account ID of the new account.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_add(const pjsua_acc_config *acc_cfg,
				   pj_bool_t is_default,
				   pjsua_acc_id *p_acc_id);


/**
 * Add a local account. A local account is used to identify local endpoint
 * instead of a specific user, and for this reason, a transport ID is needed
 * to obtain the local address information.
 *
 * @param tid		Transport ID to generate account address.
 * @param is_default	If non-zero, this account will be set as the default
 *			account. The default account will be used when sending
 *			outgoing requests (e.g. making call) when no account is
 *			specified, and when receiving incoming requests when the
 *			request does not match any accounts. It is recommended
 *			that default account is set to local/LAN account.
 * @param p_acc_id	Pointer to receive account ID of the new account.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_add_local(pjsua_transport_id tid,
					 pj_bool_t is_default,
					 pjsua_acc_id *p_acc_id);

/**
 * Set arbitrary data to be associated with the account.
 *
 * @param acc_id	The account ID.
 * @param user_data	User/application data.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_user_data(pjsua_acc_id acc_id,
					     void *user_data);


/**
 * Retrieve arbitrary data associated with the account.
 *
 * @param acc_id	The account ID.
 *
 * @return		The user data. In the case where the account ID is
 *			not valid, NULL is returned.
 */
PJ_DECL(void*) pjsua_acc_get_user_data(pjsua_acc_id acc_id);


/**
 * Delete an account. This will unregister the account from the SIP server,
 * if necessary, and terminate server side presence subscriptions associated
 * with this account.
 *
 * @param acc_id	Id of the account to be deleted.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_del(pjsua_acc_id acc_id);


/**
 * Get current config for the account. This will copy current account setting
 * to the specified parameter. Note that all pointers in the settings will
 * point to the original settings in the account and application must not
 * modify the values in any way. Application must also take care that these
 * data is only valid until the account is destroyed.
 *
 * @param acc_id	The account ID.
 * @param pool		Pool to duplicate the config.
 * @param acc_cfg	Structure to receive the settings.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_get_config(pjsua_acc_id acc_id,
                                          pj_pool_t *pool,
                                          pjsua_acc_config *acc_cfg);


/**
 * Modify account configuration setting. This function may trigger
 * unregistration (of old account setting) and re-registration (of the new
 * account setting), e.g: changing account ID, credential, registar, or
 * proxy setting.
 *
 * Note:
 * - when the new config triggers unregistration, the pjsua callback
 *   on_reg_state()/on_reg_state2() for the unregistration will not be called
 *   and any failure in the unregistration will be ignored, so if application
 *   needs to be sure about the unregistration status, it should unregister
 *   manually and wait for the callback before calling this function
 * - when the new config triggers re-registration and the re-registration
 *   fails, the account setting will not be reverted back to the old setting
 *   and the account will be in unregistered state.
 * 
 * @param acc_id	Id of the account to be modified.
 * @param acc_cfg	New account configuration.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_modify(pjsua_acc_id acc_id,
				      const pjsua_acc_config *acc_cfg);


/**
 * Modify account's presence status to be advertised to remote/presence
 * subscribers. This would trigger the sending of outgoing NOTIFY request
 * if there are server side presence subscription for this account, and/or
 * outgoing PUBLISH if presence publication is enabled for this account.
 *
 * @see pjsua_acc_set_online_status2()
 *
 * @param acc_id	The account ID.
 * @param is_online	True of false.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_online_status(pjsua_acc_id acc_id,
						 pj_bool_t is_online);

/**
 * Modify account's presence status to be advertised to remote/presence
 * subscribers. This would trigger the sending of outgoing NOTIFY request
 * if there are server side presence subscription for this account, and/or
 * outgoing PUBLISH if presence publication is enabled for this account.
 *
 * @see pjsua_acc_set_online_status()
 *
 * @param acc_id	The account ID.
 * @param is_online	True of false.
 * @param pr		Extended information in subset of RPID format
 *			which allows setting custom presence text.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_online_status2(pjsua_acc_id acc_id,
						  pj_bool_t is_online,
						  const pjrpid_element *pr);

/**
 * Update registration or perform unregistration. If registration is
 * configured for this account, then initial SIP REGISTER will be sent
 * when the account is added with #pjsua_acc_add(). Application normally
 * only need to call this function if it wants to manually update the
 * registration or to unregister from the server.
 *
 * @param acc_id	The account ID.
 * @param renew		If renew argument is zero, this will start 
 *			unregistration process.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_registration(pjsua_acc_id acc_id, 
						pj_bool_t renew);

/**
 * Get information about the specified account.
 *
 * @param acc_id	Account identification.
 * @param info		Pointer to receive account information.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_get_info(pjsua_acc_id acc_id,
					pjsua_acc_info *info);


/**
 * Enumerate all account currently active in the library. This will fill
 * the array with the account Ids, and application can then query the
 * account information for each id with #pjsua_acc_get_info().
 *
 * @see pjsua_acc_enum_info().
 *
 * @param ids		Array of account IDs to be initialized.
 * @param count		In input, specifies the maximum number of elements.
 *			On return, it contains the actual number of elements.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_accs(pjsua_acc_id ids[],
				     unsigned *count );


/**
 * Enumerate account informations.
 *
 * @param info		Array of account infos to be initialized.
 * @param count		In input, specifies the maximum number of elements.
 *			On return, it contains the actual number of elements.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_enum_info( pjsua_acc_info info[],
					  unsigned *count );


/**
 * This is an internal function to find the most appropriate account to
 * used to reach to the specified URL.
 *
 * @param url		The remote URL to reach.
 *
 * @return		Account id.
 */
PJ_DECL(pjsua_acc_id) pjsua_acc_find_for_outgoing(const pj_str_t *url);


/**
 * This is an internal function to find the most appropriate account to be
 * used to handle incoming calls.
 *
 * @param rdata		The incoming request message.
 *
 * @return		Account id.
 */
PJ_DECL(pjsua_acc_id) pjsua_acc_find_for_incoming(pjsip_rx_data *rdata);


/**
 * Create arbitrary requests using the account. Application should only use
 * this function to create auxiliary requests outside dialog, such as
 * OPTIONS, and use the call or presence API to create dialog related
 * requests.
 *
 * @param acc_id	The account ID.
 * @param method	The SIP method of the request.
 * @param target	Target URI.
 * @param p_tdata	Pointer to receive the request.
 *
 * @return		PJ_SUCCESS or the error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_create_request(pjsua_acc_id acc_id,
					      const pjsip_method *method,
					      const pj_str_t *target,
					      pjsip_tx_data **p_tdata);


/**
 * Create a suitable Contact header value, based on the specified target URI 
 * for the specified account.
 *
 * @param pool		Pool to allocate memory for the string.
 * @param contact	The string where the Contact will be stored.
 * @param acc_id	Account ID.
 * @param uri		Destination URI of the request.
 *
 * @return		PJ_SUCCESS on success, other on error.
 */
PJ_DECL(pj_status_t) pjsua_acc_create_uac_contact( pj_pool_t *pool,
						   pj_str_t *contact,
						   pjsua_acc_id acc_id,
						   const pj_str_t *uri);
							   


/**
 * Create a suitable Contact header value, based on the information in the 
 * incoming request.
 *
 * @param pool		Pool to allocate memory for the string.
 * @param contact	The string where the Contact will be stored.
 * @param acc_id	Account ID.
 * @param rdata		Incoming request.
 *
 * @return		PJ_SUCCESS on success, other on error.
 */
PJ_DECL(pj_status_t) pjsua_acc_create_uas_contact( pj_pool_t *pool,
						   pj_str_t *contact,
						   pjsua_acc_id acc_id,
						   pjsip_rx_data *rdata );
							   

/**
 * Lock/bind this account to a specific transport/listener. Normally
 * application shouldn't need to do this, as transports will be selected
 * automatically by the stack according to the destination.
 *
 * When account is locked/bound to a specific transport, all outgoing
 * requests from this account will use the specified transport (this
 * includes SIP registration, dialog (call and event subscription), and
 * out-of-dialog requests such as MESSAGE).
 *
 * Note that transport_id may be specified in pjsua_acc_config too.
 *
 * @param acc_id	The account ID.
 * @param tp_id		The transport ID.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_transport(pjsua_acc_id acc_id,
					     pjsua_transport_id tp_id);


/**
 * @}
 */


/*****************************************************************************
 * CALLS API
 */


/**
 * @defgroup PJSUA_LIB_CALL PJSUA-API Calls Management
 * @ingroup PJSUA_LIB
 * @brief Call manipulation.
 * @{
 */

/**
 * Maximum simultaneous calls.
 */
#ifndef PJSUA_MAX_CALLS
#   define PJSUA_MAX_CALLS	    4
#endif

/**
 * Maximum active video windows
 */
#ifndef PJSUA_MAX_VID_WINS
#   define PJSUA_MAX_VID_WINS	    16
#endif

/**
 * Video window ID.
 */
typedef int pjsua_vid_win_id;


/**
 * This enumeration specifies the media status of a call, and it's part
 * of pjsua_call_info structure.
 */
typedef enum pjsua_call_media_status
{
    /**
     * Call currently has no media, or the media is not used.
     */
    PJSUA_CALL_MEDIA_NONE,

    /**
     * The media is active
     */
    PJSUA_CALL_MEDIA_ACTIVE,

    /**
     * The media is currently put on hold by local endpoint
     */
    PJSUA_CALL_MEDIA_LOCAL_HOLD,

    /**
     * The media is currently put on hold by remote endpoint
     */
    PJSUA_CALL_MEDIA_REMOTE_HOLD,

    /**
     * The media has reported error (e.g. ICE negotiation)
     */
    PJSUA_CALL_MEDIA_ERROR

} pjsua_call_media_status;


/**
 * Enumeration of video keyframe request methods. Keyframe request is
 * triggered by decoder, usually when the incoming video stream cannot
 * be decoded properly due to missing video keyframe.
 */
typedef enum pjsua_vid_req_keyframe_method
{
    /**
     * Requesting keyframe via SIP INFO message. Note that incoming keyframe
     * request via SIP INFO will always be handled even if this flag is unset.
     */
    PJSUA_VID_REQ_KEYFRAME_SIP_INFO	= 1,

    /**
     * Requesting keyframe via Picture Loss Indication of RTCP feedback.
     */
    PJSUA_VID_REQ_KEYFRAME_RTCP_PLI	= 2

} pjsua_vid_req_keyframe_method;


/**
 * Call media information.
 */
typedef struct pjsua_call_media_info
{
    /** Media index in SDP. */
    unsigned		index;

    /** Media type. */
    pjmedia_type	type;

    /** Media direction. */
    pjmedia_dir		dir;

    /** Call media status. */
    pjsua_call_media_status status;

    /** The specific media stream info. */
    union {
	/** Audio stream */
	struct {
	    /** The conference port number for the call.  */
	    pjsua_conf_port_id	    conf_slot;
	} aud;

	/** Video stream */
	struct {
	    /**
	     * The window id for incoming video, if any, or
	     * PJSUA_INVALID_ID.
	     */
	    pjsua_vid_win_id	    win_in;

	    /**
	     * The video conference port number for the call in decoding
	     * direction.
	     */
	    pjsua_conf_port_id	    dec_slot;

	    /**
	     * The video conference port number for the call in encoding
	     * direction.
	     */
	    pjsua_conf_port_id	    enc_slot;

	    /**
	     * The video capture device for outgoing transmission,
	     * if any, or PJMEDIA_VID_INVALID_DEV
	     */
	    pjmedia_vid_dev_index   cap_dev;

	} vid;
    } stream;

} pjsua_call_media_info;


/**
 * This structure describes the information and current status of a call.
 */
typedef struct pjsua_call_info
{
    /** Call identification. */
    pjsua_call_id	id;

    /** Initial call role (UAC == caller) */
    pjsip_role_e	role;

    /** The account ID where this call belongs. */
    pjsua_acc_id	acc_id;

    /** Local URI */
    pj_str_t		local_info;

    /** Local Contact */
    pj_str_t		local_contact;

    /** Remote URI */
    pj_str_t		remote_info;

    /** Remote contact */
    pj_str_t		remote_contact;

    /** Dialog Call-ID string. */
    pj_str_t		call_id;

    /** Call setting */
    pjsua_call_setting	setting;

    /** Call state */
    pjsip_inv_state	state;

    /** Text describing the state */
    pj_str_t		state_text;

    /** Last status code heard, which can be used as cause code */
    pjsip_status_code	last_status;

    /** The reason phrase describing the status. */
    pj_str_t		last_status_text;

    /** Media status of the default audio stream. Default audio stream 
     *  is chosen according to this priority:
     *  1. enabled, i.e: SDP media port not zero
     *  2. transport protocol in the SDP matching account config's
     *     secure media transport usage (\a use_srtp field).
     *  3. active, i.e: SDP media direction is not "inactive"
     *  4. media order (according to the SDP).
     */
    pjsua_call_media_status media_status;

    /** Media direction of the default audio stream.
     *  See \a media_status above on how the default is chosen.
     */
    pjmedia_dir		media_dir;

    /** The conference port number for the default audio stream.
     *  See \a media_status above on how the default is chosen.
     */
    pjsua_conf_port_id	conf_slot;

    /** Number of active media info in this call. */
    unsigned		media_cnt;

    /** Array of active media information. */
    pjsua_call_media_info media[PJMEDIA_MAX_SDP_MEDIA];

    /** Number of provisional media info in this call. */
    unsigned		prov_media_cnt;

    /** Array of provisional media information. This contains the media info
     *  in the provisioning state, that is when the media session is being
     *  created/updated (SDP offer/answer is on progress).
     */
    pjsua_call_media_info prov_media[PJMEDIA_MAX_SDP_MEDIA];

    /** Up-to-date call connected duration (zero when call is not 
     *  established)
     */
    pj_time_val		connect_duration;

    /** Total call duration, including set-up time */
    pj_time_val		total_duration;

    /** Flag if remote was SDP offerer */
    pj_bool_t		rem_offerer;

    /** Number of audio streams offered by remote */
    unsigned		rem_aud_cnt;

    /** Number of video streams offered by remote */
    unsigned		rem_vid_cnt;

    /** Internal */
    struct {
	char	local_info[PJSIP_MAX_URL_SIZE];
	char	local_contact[PJSIP_MAX_URL_SIZE];
	char	remote_info[PJSIP_MAX_URL_SIZE];
	char	remote_contact[PJSIP_MAX_URL_SIZE];
	char	call_id[128];
	char	last_status_text[128];
    } buf_;

} pjsua_call_info;

/**
 * Flags to be given to various call APIs. More than one flags may be
 * specified by bitmasking them.
 */
typedef enum pjsua_call_flag
{
    /**
     * When the call is being put on hold, specify this flag to unhold it.
     * This flag is only valid for #pjsua_call_reinvite() and
     * #pjsua_call_update(). Note: for compatibility reason, this flag must
     * have value of 1 because previously the unhold option is specified as
     * boolean value.
     */
    PJSUA_CALL_UNHOLD = 1,

    /**
     * Update the local invite session's contact with the contact URI from
     * the account. This flag is only valid for #pjsua_call_set_hold2(),
     * #pjsua_call_reinvite() and #pjsua_call_update(). This flag is useful
     * in IP address change situation, after the local account's Contact has
     * been updated (typically with re-registration) use this flag to update
     * the invite session with the new Contact and to inform this new Contact
     * to the remote peer with the outgoing re-INVITE or UPDATE.
     */
    PJSUA_CALL_UPDATE_CONTACT = 2,

    /**
     * Include SDP "m=" line with port set to zero for each disabled media
     * (i.e when aud_cnt or vid_cnt is set to zero). This flag is only valid
     * for #pjsua_call_make_call(), #pjsua_call_reinvite(), and
     * #pjsua_call_update(). Note that even this flag is applicable in
     * #pjsua_call_reinvite() and #pjsua_call_update(), it will only take
     * effect when the re-INVITE/UPDATE operation regenerates SDP offer,
     * such as changing audio or video count in the call setting.
     */
    PJSUA_CALL_INCLUDE_DISABLED_MEDIA = 4,
    
    /**
     * Do not send SDP when sending INVITE or UPDATE. This flag is only valid
     * for #pjsua_call_make_call(), #pjsua_call_reinvite()/reinvite2(), or
     * #pjsua_call_update()/update2(). For re-invite/update, specifying
     * PJSUA_CALL_UNHOLD will take precedence over this flag.
     */
    PJSUA_CALL_NO_SDP_OFFER = 8,

    /**
     * Deinitialize and recreate media, including media transport. This flag
     * is useful in IP address change situation, if the media transport
     * address (or address family) changes, for example during IPv4/IPv6
     * network handover.
     * This flag is only valid for #pjsua_call_reinvite()/reinvite2(), or
     * #pjsua_call_update()/update2().
     *
     * Warning: If the re-INVITE/UPDATE fails, the old media will not be
     * reverted.
     */
    PJSUA_CALL_REINIT_MEDIA = 16,
    
    /**
     * Update the local invite session's Via with the via address from
     * the account. This flag is only valid for #pjsua_call_set_hold2(),
     * #pjsua_call_reinvite() and #pjsua_call_update(). Similar to
     * the flag PJSUA_CALL_UPDATE_CONTACT above, this flag is useful
     * in IP address change situation, after the local account's Via has
     * been updated (typically with re-registration).
     */
    PJSUA_CALL_UPDATE_VIA = 32,

    /**
     * Update dialog target to URI specified in pjsua_msg_data.target_uri.
     * This flag is only valid for pjsua_call_set_hold(),
     * pjsua_call_reinvite(), and pjsua_call_update(). This flag can be
     * useful in IP address change scenario where IP version has been changed
     * and application needs to update target IP address.
     */
    PJSUA_CALL_UPDATE_TARGET = 64,

    /**
     * Set media direction as specified in pjsua_call_setting.media_dir.
     */
    PJSUA_CALL_SET_MEDIA_DIR = 128

} pjsua_call_flag;

/**
 * This enumeration represents video stream operation on a call.
 * See also #pjsua_call_vid_strm_op_param for further info.
 */
typedef enum pjsua_call_vid_strm_op
{
    /**
     * No operation
     */
    PJSUA_CALL_VID_STRM_NO_OP,

    /**
     * Add a new video stream. This will add a new m=video line to
     * the media, regardless of whether existing video is/are present
     * or not.  This will cause re-INVITE or UPDATE to be sent to remote
     * party.
     */
    PJSUA_CALL_VID_STRM_ADD,

    /**
     * Remove/disable an existing video stream. This will
     * cause re-INVITE or UPDATE to be sent to remote party.
     */
    PJSUA_CALL_VID_STRM_REMOVE,

    /**
     * Change direction of a video stream. This operation can be used
     * to activate or deactivate an existing video media. This will
     * cause re-INVITE or UPDATE to be sent to remote party.
     */
    PJSUA_CALL_VID_STRM_CHANGE_DIR,

    /**
     * Change capture device of a video stream.  This will not send
     * re-INVITE or UPDATE to remote party.
     */
    PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV,

    /**
     * Start transmitting video stream. This will cause previously
     * stopped stream to start transmitting again. Note that no
     * re-INVITE/UPDATE is to be transmitted to remote since this
     * operation only operates on local stream.
     */
    PJSUA_CALL_VID_STRM_START_TRANSMIT,

    /**
     * Stop transmitting video stream. This will cause the stream to
     * be paused in TX direction, causing it to stop sending any video
     * packets. No re-INVITE/UPDATE is to be transmitted to remote
     * with this operation.
     */
    PJSUA_CALL_VID_STRM_STOP_TRANSMIT,

    /**
     * Send keyframe in the video stream. This will force the stream to
     * generate and send video keyframe as soon as possible. No
     * re-INVITE/UPDATE is to be transmitted to remote with this operation.
     */
    PJSUA_CALL_VID_STRM_SEND_KEYFRAME

} pjsua_call_vid_strm_op;


/**
 * Parameters for video stream operation on a call. Application should
 * use #pjsua_call_vid_strm_op_param_default() to initialize this structure
 * with its default values.
 */
typedef struct pjsua_call_vid_strm_op_param
{
    /**
     * Specify the media stream index. This can be set to -1 to denote
     * the default video stream in the call, which is the first active
     * video stream or any first video stream if none is active.
     *
     * This field is valid for all video stream operations, except
     * PJSUA_CALL_VID_STRM_ADD.
     *
     * Default: -1 (first active video stream, or any first video stream
     *              if none is active)
     */
    int med_idx;

    /**
     * Specify the media stream direction.
     *
     * This field is valid for the following video stream operations:
     * PJSUA_CALL_VID_STRM_ADD and PJSUA_CALL_VID_STRM_CHANGE_DIR.
     *
     * Default: PJMEDIA_DIR_ENCODING_DECODING
     */
    pjmedia_dir dir;

    /**
     * Specify the video capture device ID. This can be set to
     * PJMEDIA_VID_DEFAULT_CAPTURE_DEV to specify the default capture
     * device as configured in the account.
     *
     * This field is valid for the following video stream operations:
     * PJSUA_CALL_VID_STRM_ADD and PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV.
     *
     * Default: PJMEDIA_VID_DEFAULT_CAPTURE_DEV.
     */
    pjmedia_vid_dev_index cap_dev;

} pjsua_call_vid_strm_op_param;


/**
 * Specify the default signal duration when sending DTMF using SIP INFO.
 *
 * Default is 160
 */
#ifndef PJSUA_CALL_SEND_DTMF_DURATION_DEFAULT
#   define PJSUA_CALL_SEND_DTMF_DURATION_DEFAULT    160
#endif


/**
 * Parameters for sending DTMF. Application should use 
 * #pjsua_call_send_dtmf_param_default() to initialize this structure
 * with its default values.
 */
typedef struct pjsua_call_send_dtmf_param
{
    /**
     * The method used to send DTMF.
     *
     * Default: PJSUA_DTMF_METHOD_RFC2833
     */
    pjsua_dtmf_method method;

    /**
     * The signal duration used for the DTMF.
     *
     * Default: PJSUA_CALL_SEND_DTMF_DURATION_DEFAULT
     */
    unsigned duration;

    /**
     * The DTMF digits to be sent.
     */
    pj_str_t digits;

} pjsua_call_send_dtmf_param;


/**
 * Initialize call settings.
 *
 * @param opt		The call setting to be initialized.
 */
PJ_DECL(void) pjsua_call_setting_default(pjsua_call_setting *opt);


/**
 * Initialize video stream operation param with default values.
 *
 * @param param		The video stream operation param to be initialized.
 */
PJ_DECL(void)
pjsua_call_vid_strm_op_param_default(pjsua_call_vid_strm_op_param *param);


/**
 * Initialize send DTMF param with default values.
 *
 * @param param		The send DTMF param to be initialized.
 */
PJ_DECL(void) 
pjsua_call_send_dtmf_param_default(pjsua_call_send_dtmf_param *param);


/**
 * Get maximum number of calls configured in pjsua.
 *
 * @return		Maximum number of calls configured.
 */
PJ_DECL(unsigned) pjsua_call_get_max_count(void);

/**
 * Get the number of current calls. The number includes active calls
 * (pjsua_call_is_active(call_id) == PJ_TRUE), as well as calls that
 * are no longer active but still in the process of hanging up.
 *
 * @return		Number of current calls.
 */
PJ_DECL(unsigned) pjsua_call_get_count(void);

/**
 * Enumerate all active calls. Application may then query the information and
 * state of each call by calling #pjsua_call_get_info().
 *
 * @param ids		Array of account IDs to be initialized.
 * @param count		In input, specifies the maximum number of elements.
 *			On return, it contains the actual number of elements.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_calls(pjsua_call_id ids[],
				      unsigned *count);


/**
 * Make outgoing call to the specified URI using the specified account.
 *
 * @param acc_id	The account to be used.
 * @param dst_uri	URI to be put in the To header (normally is the same
 *			as the target URI).
 * @param opt		Optional call setting. This should be initialized
 *			using #pjsua_call_setting_default().
 * @param user_data	Arbitrary user data to be attached to the call, and
 *			can be retrieved later.
 * @param msg_data	Optional headers etc to be added to outgoing INVITE
 *			request, or NULL if no custom header is desired.
 * @param p_call_id	Pointer to receive call identification.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_make_call(pjsua_acc_id acc_id,
					  const pj_str_t *dst_uri,
					  const pjsua_call_setting *opt,
					  void *user_data,
					  const pjsua_msg_data *msg_data,
					  pjsua_call_id *p_call_id);


/**
 * Check if the specified call has active INVITE session and the INVITE
 * session has not been disconnected.
 *
 * @param call_id	Call identification.
 *
 * @return		Non-zero if call is active.
 */
PJ_DECL(pj_bool_t) pjsua_call_is_active(pjsua_call_id call_id);


/**
 * Check if call has an active media session.
 *
 * @param call_id	Call identification.
 *
 * @return		Non-zero if yes.
 */
PJ_DECL(pj_bool_t) pjsua_call_has_media(pjsua_call_id call_id);


/**
 * Get the conference port identification associated with the call.
 *
 * @param call_id	Call identification.
 *
 * @return		Conference port ID, or PJSUA_INVALID_ID when the
 *			media has not been established or is not active.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_call_get_conf_port(pjsua_call_id call_id);


/**
 * Get the video window associated with the call. Note that this function
 * will only evaluate the first video stream in the call, to query any other
 * video stream, use pjsua_call_get_info().
 *
 * @param call_id	Call identification.
 *
 * @return		Video window, or PJSUA_INVALID_ID when the
 *			media has not been established or is not active.
 */
PJ_DECL(pjsua_vid_win_id) pjsua_call_get_vid_win(pjsua_call_id call_id);


/**
 * Get the video conference port identification associated with the call.
 * Note that this function will only evaluate the first video stream in
 * the call, to query any other video stream, use pjsua_call_get_info().
 *
 * @param call_id	Call identification.
 * @param dir		Port direction to be queried. Valid values are
 *			PJMEDIA_DIR_ENCODING and PJMEDIA_DIR_DECODING only.
 *
 * @return		Conference port ID, or PJSUA_INVALID_ID when the
 *			media has not been established or is not active.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_call_get_vid_conf_port(
						    pjsua_call_id call_id,
						    pjmedia_dir dir);

/**
 * Obtain detail information about the specified call.
 *
 * @param call_id	Call identification.
 * @param info		Call info to be initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_get_info(pjsua_call_id call_id,
					 pjsua_call_info *info);

/**
 * Check if remote peer support the specified capability.
 *
 * @param call_id	Call identification.
 * @param htype		The header type to be checked, which value may be:
 *			- PJSIP_H_ACCEPT
 *			- PJSIP_H_ALLOW
 *			- PJSIP_H_SUPPORTED
 * @param hname		If htype specifies PJSIP_H_OTHER, then the header
 *			name must be supplied in this argument. Otherwise the 
 *			value must be set to NULL.
 * @param token		The capability token to check. For example, if \a 
 *			htype is PJSIP_H_ALLOW, then \a token specifies the 
 *			method names; if \a htype is PJSIP_H_SUPPORTED, then
 *			\a token specifies the extension names such as
 *			 "100rel".
 *
 * @return		PJSIP_DIALOG_CAP_SUPPORTED if the specified capability
 *			is explicitly supported, see pjsip_dialog_cap_status
 *			for more info.
 */
PJ_DECL(pjsip_dialog_cap_status) pjsua_call_remote_has_cap(
						    pjsua_call_id call_id,
						    int htype,
						    const pj_str_t *hname,
						    const pj_str_t *token);

/**
 * Attach application specific data to the call. Application can then
 * inspect this data by calling #pjsua_call_get_user_data().
 *
 * @param call_id	Call identification.
 * @param user_data	Arbitrary data to be attached to the call.
 *
 * @return		The user data.
 */
PJ_DECL(pj_status_t) pjsua_call_set_user_data(pjsua_call_id call_id,
					      void *user_data);


/**
 * Get user data attached to the call, which has been previously set with
 * #pjsua_call_set_user_data().
 *
 * @param call_id	Call identification.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pjsua_call_get_user_data(pjsua_call_id call_id);


/**
 * Get the NAT type of remote's endpoint. This is a proprietary feature
 * of PJSUA-LIB which sends its NAT type in the SDP when \a nat_type_in_sdp
 * is set in #pjsua_config.
 *
 * This function can only be called after SDP has been received from remote,
 * which means for incoming call, this function can be called as soon as
 * call is received as long as incoming call contains SDP, and for outgoing
 * call, this function can be called only after SDP is received (normally in
 * 200/OK response to INVITE). As a general case, application should call 
 * this function after or in \a on_call_media_state() callback.
 *
 * @param call_id	Call identification.
 * @param p_type	Pointer to store the NAT type. Application can then
 *			retrieve the string description of the NAT type
 *			by calling pj_stun_get_nat_name().
 *
 * @return		PJ_SUCCESS on success.
 *
 * @see pjsua_get_nat_type(), nat_type_in_sdp
 */
PJ_DECL(pj_status_t) pjsua_call_get_rem_nat_type(pjsua_call_id call_id,
						 pj_stun_nat_type *p_type);

/**
 * Send response to incoming INVITE request. Depending on the status
 * code specified as parameter, this function may send provisional
 * response, establish the call, or terminate the call. See also
 * #pjsua_call_answer2().
 *
 * @param call_id	Incoming call identification.
 * @param code		Status code, (100-699).
 * @param reason	Optional reason phrase. If NULL, default text
 *			will be used.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			response message. Note that this message data will
 *			be persistent in all next answers/responses for this
 *			INVITE request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_answer(pjsua_call_id call_id, 
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data);


/**
 * Send response to incoming INVITE request with call setting param.
 * Depending on the status code specified as parameter, this function may
 * send provisional response, establish the call, or terminate the call.
 * Notes about call setting:
 *  - if call setting is changed in the subsequent call to this function,
 *    only the first call setting supplied will applied. So normally
 *    application will not supply call setting before getting confirmation
 *    from the user.
 *  - if no call setting is supplied when SDP has to be sent, i.e: answer
 *    with status code 183 or 2xx, the default call setting will be used,
 *    check #pjsua_call_setting for its default values.
 *
 * @param call_id	Incoming call identification.
 * @param opt		Optional call setting.
 * @param code		Status code, (100-699).
 * @param reason	Optional reason phrase. If NULL, default text
 *			will be used.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			response message. Note that this message data will
 *			be persistent in all next answers/responses for this
 *			INVITE request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_answer2(pjsua_call_id call_id, 
					const pjsua_call_setting *opt,
				        unsigned code,
				        const pj_str_t *reason,
				        const pjsua_msg_data *msg_data);


/**
 * Same as #pjsua_call_answer2() but this function will set the SDP
 * answer first before sending the response.
 *
 * @param call_id	Incoming call identification.
 * @param sdp		SDP answer. 
 * @param opt		Optional call setting.
 * @param code		Status code, (100-699).
 * @param reason	Optional reason phrase. If NULL, default text
 *			will be used.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			response message. Note that this message data will
 *			be persistent in all next answers/responses for this
 *			INVITE request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjsua_call_answer_with_sdp(pjsua_call_id call_id,
			   const pjmedia_sdp_session *sdp, 
			   const pjsua_call_setting *opt,
			   unsigned code,
			   const pj_str_t *reason,
			   const pjsua_msg_data *msg_data);


/**
 * Hangup call by using method that is appropriate according to the
 * call state. This function is different than answering the call with
 * 3xx-6xx response (with #pjsua_call_answer()), in that this function
 * will hangup the call regardless of the state and role of the call,
 * while #pjsua_call_answer() only works with incoming calls on EARLY
 * state.
 *
 * After calling this function, media will be deinitialized (call media
 * callbacks, if any, will still be received) and then, on_call_state()
 * will be immediately called with state DISCONNECTED. No further
 * call callbacks will be received after this. The call hangup process
 * itself (sending BYE, waiting for the response, and resource cleanup)
 * will continue in the background and the call slot can be reused only
 * after this process is completed. If application has limited call slots
 * and would like to check if there are any free slots remaining, it can
 * query the number of free slots using the APIs:
 * pjsua_call_get_max_count()-pjsua_call_get_count()
 *
 * Note that on_call_tsx_state() will not be called when using this API.
 *
 * @param call_id	Call identification.
 * @param code		Optional status code to be sent when we're rejecting
 *			incoming call. If the value is zero, "603/Decline"
 *			will be sent.
 * @param reason	Optional reason phrase to be sent when we're rejecting
 *			incoming call.  If NULL, default text will be used.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			request/response message.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_hangup(pjsua_call_id call_id,
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data);

/**
 * Accept or reject redirection response. Application MUST call this function
 * after it signaled PJSIP_REDIRECT_PENDING in the \a on_call_redirected() 
 * callback, to notify the call whether to accept or reject the redirection
 * to the current target. Application can use the combination of
 * PJSIP_REDIRECT_PENDING command in \a on_call_redirected() callback and 
 * this function to ask for user permission before redirecting the call.
 *
 * Note that if the application chooses to reject or stop redirection (by 
 * using PJSIP_REDIRECT_REJECT or PJSIP_REDIRECT_STOP respectively), the
 * call disconnection callback will be called before this function returns.
 * And if the application rejects the target, the \a on_call_redirected() 
 * callback may also be called before this function returns if there is 
 * another target to try.
 *
 * @param call_id	The call ID.
 * @param cmd		Redirection operation to be applied to the current
 *			target. The semantic of this argument is similar
 *			to the description in the \a on_call_redirected()
 *			callback, except that the PJSIP_REDIRECT_PENDING is
 *			not accepted here.
 *
 * @return		PJ_SUCCESS on successful operation.
 */
PJ_DECL(pj_status_t) pjsua_call_process_redirect(pjsua_call_id call_id,
						 pjsip_redirect_op cmd);

/**
 * Put the specified call on hold. This will send re-INVITE with the
 * appropriate SDP to inform remote that the call is being put on hold.
 * The final status of the request itself will be reported on the
 * \a on_call_media_state() callback, which inform the application that
 * the media state of the call has changed.
 *
 * @param call_id	Call identification.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_set_hold(pjsua_call_id call_id,
					 const pjsua_msg_data *msg_data);

/**
 * Put the specified call on hold. This will send re-INVITE with the
 * appropriate SDP to inform remote that the call is being put on hold.
 * The final status of the request itself will be reported on the
 * \a on_call_media_state() callback, which inform the application that
 * the media state of the call has changed.
 *
 * @param call_id	Call identification.
 * @param options	Bitmask of pjsua_call_flag constants. Currently, only
 *                      the flag PJSUA_CALL_UPDATE_CONTACT can be used.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_set_hold2(pjsua_call_id call_id,
                                          unsigned options,
					  const pjsua_msg_data *msg_data);

/**
 * Send re-INVITE request or release hold.
 * The final status of the request itself will be reported on the
 * \a on_call_media_state() callback, which inform the application that
 * the media state of the call has changed.
 *
 * @param call_id	Call identification.
 * @param options	Bitmask of pjsua_call_flag constants. Note that
 * 			for compatibility, specifying PJ_TRUE here is
 * 			equal to specifying PJSUA_CALL_UNHOLD flag.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_reinvite(pjsua_call_id call_id,
					 unsigned options,
					 const pjsua_msg_data *msg_data);


/**
 * Send re-INVITE request or release hold.
 * The final status of the request itself will be reported on the
 * \a on_call_media_state() callback, which inform the application that
 * the media state of the call has changed.
 *
 * @param call_id	Call identification.
 * @param opt		Optional call setting, if NULL, the current call
 *			setting will be used. Note that to release hold
 *			or update contact or omit SDP offer, this parameter
 *			cannot be NULL and it must specify appropriate flags,
 *			e.g: PJSUA_CALL_UNHOLD, PJSUA_CALL_UPDATE_CONTACT,
 *			PJSUA_CALL_NO_SDP_OFFER.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_reinvite2(pjsua_call_id call_id,
					  const pjsua_call_setting *opt,
					  const pjsua_msg_data *msg_data);


/**
 * Send UPDATE request.
 *
 * @param call_id	Call identification.
 * @param options	Bitmask of pjsua_call_flag constants.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_update(pjsua_call_id call_id,
				       unsigned options,
				       const pjsua_msg_data *msg_data);


/**
 * Send UPDATE request.
 *
 * @param call_id	Call identification.
 * @param opt		Optional call setting, if NULL, the current call
 *			setting will be used. Note that to release hold
 *			or update contact or omit SDP offer, this parameter
 *			cannot be NULL and it must specify appropriate flags,
 *			e.g: PJSUA_CALL_UNHOLD, PJSUA_CALL_UPDATE_CONTACT,
 *			PJSUA_CALL_NO_SDP_OFFER.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_update2(pjsua_call_id call_id,
				        const pjsua_call_setting *opt,
				        const pjsua_msg_data *msg_data);


/**
 * Initiate call transfer to the specified address. This function will send
 * REFER request to instruct remote call party to initiate a new INVITE
 * session to the specified destination/target.
 *
 * If application is interested to monitor the successfulness and 
 * the progress of the transfer request, it can implement 
 * \a on_call_transfer_status() callback which will report the progress
 * of the call transfer request.
 *
 * @param call_id	The call id to be transferred.
 * @param dest		URI of new target to be contacted. The URI may be
 * 			in name address or addr-spec format.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_xfer(pjsua_call_id call_id, 
				     const pj_str_t *dest,
				     const pjsua_msg_data *msg_data);

/**
 * Flag to indicate that "Require: replaces" should not be put in the
 * outgoing INVITE request caused by REFER request created by 
 * #pjsua_call_xfer_replaces().
 */
#define PJSUA_XFER_NO_REQUIRE_REPLACES	1

/**
 * Initiate attended call transfer. This function will send REFER request
 * to instruct remote call party to initiate new INVITE session to the URL
 * of \a dest_call_id. The party at \a dest_call_id then should "replace"
 * the call with us with the new call from the REFER recipient.
 *
 * @param call_id	The call id to be transferred.
 * @param dest_call_id	The call id to be replaced.
 * @param options	Application may specify PJSUA_XFER_NO_REQUIRE_REPLACES
 *			to suppress the inclusion of "Require: replaces" in
 *			the outgoing INVITE request created by the REFER
 *			request.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_xfer_replaces(pjsua_call_id call_id, 
					      pjsua_call_id dest_call_id,
					      unsigned options,
					      const pjsua_msg_data *msg_data);

/**
 * Send DTMF digits to remote using RFC 2833 payload formats. Use 
 * #pjsua_call_send_dtmf() to send DTMF using SIP INFO or other method in 
 * \a pjsua_dtmf_method. App can use \a on_dtmf_digit() or \a on_dtmf_digit2() 
 * callback to monitor incoming DTMF.
 *
 * @param call_id	Call identification.
 * @param digits	DTMF string digits to be sent as described on RFC 2833 
 *			section 3.10. If PJMEDIA_HAS_DTMF_FLASH is enabled, 
 *			character 'R' is used to represent the 
 *			event type 16 (flash) as stated in RFC 4730.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_dial_dtmf(pjsua_call_id call_id, 
					  const pj_str_t *digits);

/**
 * Send DTMF digits to remote. Use this method to send DTMF using the method in
 * \a pjsua_dtmf_method. This method will call #pjsua_call_dial_dtmf() when
 * sending DTMF using \a PJSUA_DTMF_METHOD_RFC2833. Note that 
 * \a on_dtmf_digit() callback can only monitor incoming DTMF using RFC 2833. 
 * App can use \a on_dtmf_digit2() to monitor incoming DTMF using the method in 
 * \a pjsua_dtmf_method. Note that \a on_dtmf_digit() will not be called once
 * \a on_dtmf_digit2() is implemented.
 *
 * @param call_id	Call identification.
 * @param param		The send DTMF parameter.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_send_dtmf(pjsua_call_id call_id, 
			              const pjsua_call_send_dtmf_param *param);

/**
 * Send instant messaging inside INVITE session.
 *
 * @param call_id	Call identification.
 * @param mime_type	Optional MIME type. If NULL, then "text/plain" is 
 *			assumed.
 * @param content	The message content. Can be NULL if msg_data specifies
 *			body and/or multipart.
 * @param msg_data	Optional list of headers etc to be included in outgoing
 *			request. The body descriptor in the msg_data is 
 *			ignored if parameter 'content' is set.
 * @param user_data	Optional user data, which will be given back when
 *			the IM callback is called.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_send_im( pjsua_call_id call_id, 
					 const pj_str_t *mime_type,
					 const pj_str_t *content,
					 const pjsua_msg_data *msg_data,
					 void *user_data);


/**
 * Send IM typing indication inside INVITE session.
 *
 * @param call_id	Call identification.
 * @param is_typing	Non-zero to indicate to remote that local person is
 *			currently typing an IM.
 * @param msg_data	Optional list of headers etc to be included in outgoing
 *			request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_send_typing_ind(pjsua_call_id call_id, 
						pj_bool_t is_typing,
						const pjsua_msg_data*msg_data);

/**
 * Send arbitrary request with the call. This is useful for example to send
 * INFO request. Note that application should not use this function to send
 * requests which would change the invite session's state, such as re-INVITE,
 * UPDATE, PRACK, and BYE.
 *
 * @param call_id	Call identification.
 * @param method	SIP method of the request.
 * @param msg_data	Optional message body and/or list of headers to be 
 *			included in outgoing request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_send_request(pjsua_call_id call_id,
					     const pj_str_t *method,
					     const pjsua_msg_data *msg_data);


/**
 * Terminate all calls. This will initiate #pjsua_call_hangup() for all
 * currently active calls. 
 */
PJ_DECL(void) pjsua_call_hangup_all(void);


/**
 * Dump call and media statistics to string.
 *
 * @param call_id	Call identification.
 * @param with_media	Non-zero to include media information too.
 * @param buffer	Buffer where the statistics are to be written to.
 * @param maxlen	Maximum length of buffer.
 * @param indent	Spaces for left indentation.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_call_dump(pjsua_call_id call_id, 
				     pj_bool_t with_media, 
				     char *buffer, 
				     unsigned maxlen,
				     const char *indent);

/**
 * Get the media stream index of the default video stream in the call.
 * Typically this will just retrieve the stream index of the first
 * activated video stream in the call. If none is active, it will return
 * the first inactive video stream.
 *
 * @param call_id	Call identification.
 *
 * @return		The media stream index or -1 if no video stream
 * 			is present in the call.
 */
PJ_DECL(int) pjsua_call_get_vid_stream_idx(pjsua_call_id call_id);


/**
 * Determine if video stream for the specified call is currently running
 * (i.e. has been created, started, and not being paused) for the specified
 *  direction.
 *
 * @param call_id	Call identification.
 * @param med_idx	Media stream index, or -1 to specify default video
 * 			media.
 * @param dir		The direction to be checked.
 *
 * @return		PJ_TRUE if stream is currently running for the
 * 			specified direction.
 */
PJ_DECL(pj_bool_t) pjsua_call_vid_stream_is_running(pjsua_call_id call_id,
                                                    int med_idx,
                                                    pjmedia_dir dir);

/**
 * Add, remove, modify, and/or manipulate video media stream for the
 * specified call. This may trigger a re-INVITE or UPDATE to be sent
 * for the call.
 *
 * @param call_id	Call identification.
 * @param op		The video stream operation to be performed,
 *			possible values are #pjsua_call_vid_strm_op.
 * @param param		The parameters for the video stream operation,
 *			or NULL for the default parameter values
 *			(see #pjsua_call_vid_strm_op_param).
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t) pjsua_call_set_vid_strm (
				pjsua_call_id call_id,
				pjsua_call_vid_strm_op op,
				const pjsua_call_vid_strm_op_param *param);

/**
 * Modify the audio stream's codec parameter after the codec is opened.
 * Note that not all codec parameters can be modified during run-time.
 * Currently, only Opus codec supports changing key codec parameters
 * such as bitrate and bandwidth, while other codecs may only be able to
 * modify minor settings such as VAD or PLC.
 *
 * @param call_id	Call identification.
 * @param med_idx	Media stream index, or -1 to specify default audio
 * 			media.
 * @param param		The new codec parameter.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjsua_call_aud_stream_modify_codec_param(pjsua_call_id call_id,
                                         int med_idx,
			  	  	 const pjmedia_codec_param *param);

/**
 * Get media stream info for the specified media index.
 *
 * @param call_id	The call identification.
 * @param med_idx	Media stream index.
 * @param psi		To be filled with the stream info.
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t) pjsua_call_get_stream_info(pjsua_call_id call_id,
                                                unsigned med_idx,
                                                pjsua_stream_info *psi);

/**
 *  Get media stream statistic for the specified media index.
 *
 * @param call_id	The call identification.
 * @param med_idx	Media stream index.
 * @param stat		To be filled with the stream statistic.
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t) pjsua_call_get_stream_stat(pjsua_call_id call_id,
                                                unsigned med_idx,
                                                pjsua_stream_stat *stat);

/**
 * Get media transport info for the specified media index.
 *
 * @param call_id	The call identification.
 * @param med_idx	Media stream index.
 * @param t		To be filled with the transport info.
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t) 
pjsua_call_get_med_transport_info(pjsua_call_id call_id,
                                  unsigned med_idx,
                                  pjmedia_transport_info *t);



/**
 * @}
 */


/*****************************************************************************
 * BUDDY API
 */


/**
 * @defgroup PJSUA_LIB_BUDDY PJSUA-API Buddy, Presence, and Instant Messaging
 * @ingroup PJSUA_LIB
 * @brief Buddy management, buddy's presence, and instant messaging.
 * @{
 *
 * This section describes PJSUA-APIs related to buddies management,
 * presence management, and instant messaging.
 */

/**
 * Max buddies in buddy list.
 */
#ifndef PJSUA_MAX_BUDDIES
#   define PJSUA_MAX_BUDDIES	    256
#endif


/**
 * This specifies how long the library should wait before retrying failed
 * SUBSCRIBE request, and there is no rule to automatically resubscribe 
 * (for example, no "retry-after" parameter in Subscription-State header).
 *
 * This also controls the duration  before failed PUBLISH request will be
 * retried.
 *
 * Default: 300 seconds
 */
#ifndef PJSUA_PRES_TIMER
#   define PJSUA_PRES_TIMER	    300
#endif


/**
 * This structure describes buddy configuration when adding a buddy to
 * the buddy list with #pjsua_buddy_add(). Application MUST initialize
 * the structure with #pjsua_buddy_config_default() to initialize this
 * structure with default configuration.
 */
typedef struct pjsua_buddy_config
{
    /**
     * Buddy URL or name address.
     */
    pj_str_t	uri;

    /**
     * Specify whether presence subscription should start immediately.
     */
    pj_bool_t	subscribe;

    /**
     * Specify arbitrary application data to be associated with with
     * the buddy object.
     */
    void       *user_data;

} pjsua_buddy_config;


/**
 * This enumeration describes basic buddy's online status.
 */
typedef enum pjsua_buddy_status
{
    /**
     * Online status is unknown (possibly because no presence subscription
     * has been established).
     */
    PJSUA_BUDDY_STATUS_UNKNOWN,

    /**
     * Buddy is known to be online.
     */
    PJSUA_BUDDY_STATUS_ONLINE,

    /**
     * Buddy is offline.
     */
    PJSUA_BUDDY_STATUS_OFFLINE,

} pjsua_buddy_status;



/**
 * This structure describes buddy info, which can be retrieved by calling
 * #pjsua_buddy_get_info().
 */
typedef struct pjsua_buddy_info
{
    /**
     * The buddy ID.
     */
    pjsua_buddy_id	id;

    /**
     * The full URI of the buddy, as specified in the configuration.
     */
    pj_str_t		uri;

    /**
     * Buddy's Contact, only available when presence subscription has
     * been established to the buddy.
     */
    pj_str_t		contact;

    /**
     * Buddy's online status.
     */
    pjsua_buddy_status	status;

    /**
     * Text to describe buddy's online status.
     */
    pj_str_t		status_text;

    /**
     * Flag to indicate that we should monitor the presence information for
     * this buddy (normally yes, unless explicitly disabled).
     */
    pj_bool_t		monitor_pres;

    /**
     * If \a monitor_pres is enabled, this specifies the last state of the
     * presence subscription. If presence subscription session is currently
     * active, the value will be PJSIP_EVSUB_STATE_ACTIVE. If presence
     * subscription request has been rejected, the value will be
     * PJSIP_EVSUB_STATE_TERMINATED, and the termination reason will be
     * specified in \a sub_term_reason.
     */
    pjsip_evsub_state	sub_state;

    /**
     * String representation of subscription state.
     */
    const char	       *sub_state_name;

    /**
     * Specifies the last presence subscription termination code. This would
     * return the last status of the SUBSCRIBE request. If the subscription
     * is terminated with NOTIFY by the server, this value will be set to
     * 200, and subscription termination reason will be given in the
     * \a sub_term_reason field.
     */
    unsigned		sub_term_code;

    /**
     * Specifies the last presence subscription termination reason. If 
     * presence subscription is currently active, the value will be empty.
     */
    pj_str_t		sub_term_reason;

    /**
     * Extended RPID information about the person.
     */
    pjrpid_element	rpid;

    /**
     * Extended presence info.
     */
    pjsip_pres_status	pres_status;

    /**
     * Internal buffer.
     */
    char		buf_[512];

} pjsua_buddy_info;


/**
 * Set default values to the buddy config.
 */
PJ_DECL(void) pjsua_buddy_config_default(pjsua_buddy_config *cfg);


/**
 * Get total number of buddies.
 *
 * @return		Number of buddies.
 */
PJ_DECL(unsigned) pjsua_get_buddy_count(void);


/**
 * Check if buddy ID is valid.
 *
 * @param buddy_id	Buddy ID to check.
 *
 * @return		Non-zero if buddy ID is valid.
 */
PJ_DECL(pj_bool_t) pjsua_buddy_is_valid(pjsua_buddy_id buddy_id);


/**
 * Enumerate all buddy IDs in the buddy list. Application then can use
 * #pjsua_buddy_get_info() to get the detail information for each buddy
 * id.
 *
 * @param ids		Array of ids to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_buddies(pjsua_buddy_id ids[],
					unsigned *count);

/**
 * Find the buddy ID with the specified URI.
 *
 * @param uri		The buddy URI.
 *
 * @return		The buddy ID, or PJSUA_INVALID_ID if not found.
 */
PJ_DECL(pjsua_buddy_id) pjsua_buddy_find(const pj_str_t *uri);


/**
 * Get detailed buddy info.
 *
 * @param buddy_id	The buddy identification.
 * @param info		Pointer to receive information about buddy.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_get_info(pjsua_buddy_id buddy_id,
					  pjsua_buddy_info *info);

/**
 * Set the user data associated with the buddy object.
 *
 * @param buddy_id	The buddy identification.
 * @param user_data	Arbitrary application data to be associated with
 *			the buddy object.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_set_user_data(pjsua_buddy_id buddy_id,
					       void *user_data);


/**
 * Get the user data associated with the budy object.
 *
 * @param buddy_id	The buddy identification.
 *
 * @return		The application data.
 */
PJ_DECL(void*) pjsua_buddy_get_user_data(pjsua_buddy_id buddy_id);


/**
 * Add new buddy to the buddy list. If presence subscription is enabled
 * for this buddy, this function will also start the presence subscription
 * session immediately.
 *
 * @param buddy_cfg	Buddy configuration.
 * @param p_buddy_id	Pointer to receive buddy ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_add(const pjsua_buddy_config *buddy_cfg,
				     pjsua_buddy_id *p_buddy_id);


/**
 * Delete the specified buddy from the buddy list. Any presence subscription
 * to this buddy will be terminated.
 *
 * @param buddy_id	Buddy identification.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_del(pjsua_buddy_id buddy_id);


/**
 * Enable/disable buddy's presence monitoring. Once buddy's presence is
 * subscribed, application will be informed about buddy's presence status
 * changed via \a on_buddy_state() callback.
 *
 * @param buddy_id	Buddy identification.
 * @param subscribe	Specify non-zero to activate presence subscription to
 *			the specified buddy.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_subscribe_pres(pjsua_buddy_id buddy_id,
						pj_bool_t subscribe);


/**
 * Update the presence information for the buddy. Although the library
 * periodically refreshes the presence subscription for all buddies, some
 * application may want to refresh the buddy's presence subscription
 * immediately, and in this case it can use this function to accomplish
 * this.
 *
 * Note that the buddy's presence subscription will only be initiated
 * if presence monitoring is enabled for the buddy. See 
 * #pjsua_buddy_subscribe_pres() for more info. Also if presence subscription
 * for the buddy is already active, this function will not do anything.
 *
 * Once the presence subscription is activated successfully for the buddy,
 * application will be notified about the buddy's presence status in the
 * on_buddy_state() callback.
 *
 * @param buddy_id	Buddy identification.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_update_pres(pjsua_buddy_id buddy_id);


/**
 * Send NOTIFY to inform account presence status or to terminate server
 * side presence subscription. If application wants to reject the incoming
 * request, it should set the \a state to PJSIP_EVSUB_STATE_TERMINATED.
 *
 * @param acc_id	Account ID.
 * @param srv_pres	Server presence subscription instance.
 * @param state		New state to set.
 * @param state_str	Optionally specify the state string name, if state
 *			is not "active", "pending", or "terminated".
 * @param reason	If the new state is PJSIP_EVSUB_STATE_TERMINATED,
 *			optionally specify the termination reason. 
 * @param with_body	If the new state is PJSIP_EVSUB_STATE_TERMINATED,
 *			this specifies whether the NOTIFY request should
 *			contain message body containing account's presence
 *			information.
 * @param msg_data	Optional list of headers to be sent with the NOTIFY
 *			request.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_pres_notify(pjsua_acc_id acc_id,
				       pjsua_srv_pres *srv_pres,
				       pjsip_evsub_state state,
				       const pj_str_t *state_str,
				       const pj_str_t *reason,
				       pj_bool_t with_body,
				       const pjsua_msg_data *msg_data);

/**
 * Dump presence subscriptions to log.
 *
 * @param verbose	Yes or no.
 */
PJ_DECL(void) pjsua_pres_dump(pj_bool_t verbose);


/**
 * The MESSAGE method (defined in pjsua_im.c)
 */
extern const pjsip_method pjsip_message_method;


/**
 * The INFO method (defined in pjsua_call.c)
 */
extern const pjsip_method pjsip_info_method;


/**
 * Send instant messaging outside dialog, using the specified account for
 * route set and authentication.
 *
 * @param acc_id	Account ID to be used to send the request.
 * @param to		Remote URI.
 * @param mime_type	Optional MIME type. If NULL, then "text/plain" is 
 *			assumed.
 * @param content	The message content. Can be NULL if msg_data specifies
 *			body and/or multipart.
 * @param msg_data	Optional list of headers etc to be included in outgoing
 *			request. The body descriptor in the msg_data is 
 *			ignored if parameter 'content' is set.
 * @param user_data	Optional user data, which will be given back when
 *			the IM callback is called.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_im_send(pjsua_acc_id acc_id, 
				   const pj_str_t *to,
				   const pj_str_t *mime_type,
				   const pj_str_t *content,
				   const pjsua_msg_data *msg_data,
				   void *user_data);


/**
 * Send typing indication outside dialog.
 *
 * @param acc_id	Account ID to be used to send the request.
 * @param to		Remote URI.
 * @param is_typing	If non-zero, it tells remote person that local person
 *			is currently composing an IM.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_im_typing(pjsua_acc_id acc_id, 
				     const pj_str_t *to, 
				     pj_bool_t is_typing,
				     const pjsua_msg_data *msg_data);



/**
 * @}
 */


/*****************************************************************************
 * MEDIA API
 */


/**
 * @defgroup PJSUA_LIB_MEDIA PJSUA-API Media Manipulation
 * @ingroup PJSUA_LIB
 * @brief Media manipulation.
 * @{
 *
 * PJSUA has rather powerful media features, which are built around the
 * PJMEDIA conference bridge. Basically, all media "ports" (such as calls, WAV 
 * players, WAV playlist, file recorders, sound device, tone generators, etc)
 * are terminated in the conference bridge, and application can manipulate
 * the interconnection between these terminations freely. 
 *
 * The conference bridge provides powerful switching and mixing functionality
 * for application. With the conference bridge, each conference slot (e.g. 
 * a call) can transmit to multiple destinations, and one destination can
 * receive from multiple sources. If more than one media terminations are 
 * terminated in the same slot, the conference bridge will mix the signal 
 * automatically.
 *
 * Application connects one media termination/slot to another by calling
 * #pjsua_conf_connect() function. This will establish <b>unidirectional</b>
 * media flow from the source termination to the sink termination. To
 * establish bidirectional media flow, application wound need to make another
 * call to #pjsua_conf_connect(), this time inverting the source and 
 * destination slots in the parameter.
 *
 * For example, to stream a WAV file to remote call, application may use
 * the following steps:
 *
 \code
  
  pj_status_t stream_to_call( pjsua_call_id call_id )
  {
     pjsua_player_id player_id;
     
     status = pjsua_player_create("mysong.wav", 0, &player_id);
     if (status != PJ_SUCCESS)
        return status;

     status = pjsua_conf_connect( pjsua_player_get_conf_port(),
				  pjsua_call_get_conf_port() );
  }
 \endcode
 *
 *
 * Other features of PJSUA media:
 *  - efficient N to M interconnections between media terminations.
 *  - media termination can be connected to itself to create loopback
 *    media.
 *  - the media termination may have different clock rates, and resampling
 *    will be done automatically by conference bridge.
 *  - media terminations may also have different frame time; the
 *    conference bridge will perform the necessary bufferring to adjust
 *    the difference between terminations.
 *  - interconnections are removed automatically when media termination
 *    is removed from the bridge.
 *  - sound device may be changed even when there are active media 
 *    interconnections.
 *  - correctly report call's media quality (in #pjsua_call_dump()) from
 *    RTCP packet exchange.
 */

/**
 * Use PJMEDIA for media? Set this to zero when using third party media
 * stack.
 */
#ifndef PJSUA_MEDIA_HAS_PJMEDIA
#  define PJSUA_MEDIA_HAS_PJMEDIA	1
#endif	/* PJSUA_MEDIA_HAS_PJMEDIA */


/**
 * Specify whether the third party stream has the capability of retrieving
 * the stream info, i.e: pjmedia_stream_get_info() and
 * pjmedia_vid_stream_get_info(). Currently this capability is required
 * by smart media update and call dump.
 */
#ifndef PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO
#   define PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO    0
#endif


/**
 * Specify whether the third party stream has the capability of retrieving
 * the stream statistics, i.e: pjmedia_stream_get_stat() and
 * pjmedia_vid_stream_get_stat(). Currently this capability is required
 * by call dump.
 */
#ifndef PJSUA_THIRD_PARTY_STREAM_HAS_GET_STAT
#   define PJSUA_THIRD_PARTY_STREAM_HAS_GET_STAT    0
#endif


/**
 * Max ports in the conference bridge. This setting is the default value
 * for pjsua_media_config.max_media_ports.
 */
#ifndef PJSUA_MAX_CONF_PORTS
#   define PJSUA_MAX_CONF_PORTS		254
#endif

/**
 * The default clock rate to be used by the conference bridge. This setting
 * is the default value for pjsua_media_config.clock_rate.
 */
#ifndef PJSUA_DEFAULT_CLOCK_RATE
#   define PJSUA_DEFAULT_CLOCK_RATE	16000
#endif

/**
 * Default frame length in the conference bridge. This setting
 * is the default value for pjsua_media_config.audio_frame_ptime.
 */
#ifndef PJSUA_DEFAULT_AUDIO_FRAME_PTIME
#   define PJSUA_DEFAULT_AUDIO_FRAME_PTIME  20
#endif


/**
 * Default codec quality settings. This setting is the default value
 * for pjsua_media_config.quality.
 */
#ifndef PJSUA_DEFAULT_CODEC_QUALITY
#   define PJSUA_DEFAULT_CODEC_QUALITY	8
#endif

/**
 * Default iLBC mode. This setting is the default value for 
 * pjsua_media_config.ilbc_mode.
 */
#ifndef PJSUA_DEFAULT_ILBC_MODE
#   define PJSUA_DEFAULT_ILBC_MODE	30
#endif

/**
 * The default echo canceller tail length. This setting
 * is the default value for pjsua_media_config.ec_tail_len.
 */
#ifndef PJSUA_DEFAULT_EC_TAIL_LEN
#   define PJSUA_DEFAULT_EC_TAIL_LEN	200
#endif


/**
 * The maximum file player.
 */
#ifndef PJSUA_MAX_PLAYERS
#   define PJSUA_MAX_PLAYERS		32
#endif


/**
 * The maximum file player.
 */
#ifndef PJSUA_MAX_RECORDERS
#   define PJSUA_MAX_RECORDERS		32
#endif


/**
 * Enable/disable "c=" line in SDP session level. Set to zero to disable it.
 */
#ifndef PJSUA_SDP_SESS_HAS_CONN
#   define PJSUA_SDP_SESS_HAS_CONN	0
#endif


/**
 * Specify the delay needed when restarting the transport/listener.
 * e.g: 10 msec on Linux or Android, and 0 on the other platforms.
 */
#ifndef PJSUA_TRANSPORT_RESTART_DELAY_TIME
#   define PJSUA_TRANSPORT_RESTART_DELAY_TIME	10
#endif


/**
 * This structure describes media configuration, which will be specified
 * when calling #pjsua_init(). Application MUST initialize this structure
 * by calling #pjsua_media_config_default().
 */
struct pjsua_media_config
{
    /**
     * Clock rate to be applied to the conference bridge.
     * If value is zero, default clock rate will be used 
     * (PJSUA_DEFAULT_CLOCK_RATE, which by default is 16KHz).
     */
    unsigned		clock_rate;

    /**
     * Clock rate to be applied when opening the sound device.
     * If value is zero, conference bridge clock rate will be used.
     */
    unsigned		snd_clock_rate;

    /**
     * Channel count be applied when opening the sound device and
     * conference bridge.
     */
    unsigned		channel_count;

    /**
     * Specify audio frame ptime. The value here will affect the 
     * samples per frame of both the sound device and the conference
     * bridge. Specifying lower ptime will normally reduce the
     * latency.
     *
     * Default value: PJSUA_DEFAULT_AUDIO_FRAME_PTIME
     */
    unsigned		audio_frame_ptime;

    /**
     * Specify maximum number of media ports to be created in the
     * conference bridge. Since all media terminate in the bridge
     * (calls, file player, file recorder, etc), the value must be
     * large enough to support all of them. However, the larger
     * the value, the more computations are performed.
     *
     * Default value: PJSUA_MAX_CONF_PORTS
     */
    unsigned		max_media_ports;

    /**
     * Specify whether the media manager should manage its own
     * ioqueue for the RTP/RTCP sockets. If yes, ioqueue will be created
     * and at least one worker thread will be created too. If no,
     * the RTP/RTCP sockets will share the same ioqueue as SIP sockets,
     * and no worker thread is needed.
     *
     * Normally application would say yes here, unless it wants to
     * run everything from a single thread.
     */
    pj_bool_t		has_ioqueue;

    /**
     * Specify the number of worker threads to handle incoming RTP
     * packets. A value of one is recommended for most applications.
     */
    unsigned		thread_cnt;

    /**
     * Media quality, 0-10, according to this table:
     *   5-10: resampling use large filter,
     *   3-4:  resampling use small filter,
     *   1-2:  resampling use linear.
     * The media quality also sets speex codec quality/complexity to the
     * number.
     *
     * Default: 5 (PJSUA_DEFAULT_CODEC_QUALITY).
     */
    unsigned		quality;

    /**
     * Specify default codec ptime.
     *
     * Default: 0 (codec specific)
     */
    unsigned		ptime;

    /**
     * Disable VAD?
     *
     * Default: 0 (codec specific)
     */
    pj_bool_t		no_vad;

    /**
     * iLBC mode (20 or 30).
     *
     * Default: 30 (PJSUA_DEFAULT_ILBC_MODE)
     */
    unsigned		ilbc_mode;

    /**
     * Percentage of RTP packet to drop in TX direction
     * (to simulate packet lost).
     *
     * Default: 0
     */
    unsigned		tx_drop_pct;

    /**
     * Percentage of RTP packet to drop in RX direction
     * (to simulate packet lost).
     *
     * Default: 0
     */
    unsigned		rx_drop_pct;

    /**
     * Echo canceller options (see #pjmedia_echo_create()).
     * Specify PJMEDIA_ECHO_USE_SW_ECHO here if application wishes
     * to use software echo canceller instead of device EC.
     *
     * Default: 0.
     */
    unsigned		ec_options;

    /**
     * Echo canceller tail length, in miliseconds.
     *
     * Default: PJSUA_DEFAULT_EC_TAIL_LEN
     */
    unsigned		ec_tail_len;

    /**
     * Audio capture buffer length, in milliseconds.
     *
     * Default: PJMEDIA_SND_DEFAULT_REC_LATENCY
     */
    unsigned		snd_rec_latency;

    /**
     * Audio playback buffer length, in milliseconds.
     *
     * Default: PJMEDIA_SND_DEFAULT_PLAY_LATENCY
     */
    unsigned		snd_play_latency;

    /** 
     * Jitter buffer initial prefetch delay in msec. The value must be
     * between jb_min_pre and jb_max_pre below. If the value is 0,
     * prefetching will be disabled.
     *
     * Default: -1 (to use default stream settings, currently 0)
     */
    int			jb_init;

    /**
     * Jitter buffer minimum prefetch delay in msec.
     *
     * Default: -1 (to use default stream settings, currently codec ptime)
     */
    int			jb_min_pre;
    
    /**
     * Jitter buffer maximum prefetch delay in msec.
     *
     * Default: -1 (to use default stream settings, currently 80% of jb_max)
     */
    int			jb_max_pre;

    /**
     * Set maximum delay that can be accomodated by the jitter buffer msec.
     *
     * Default: -1 (to use default stream settings, currently 500 msec)
     */
    int			jb_max;

    /**
     * Set the algorithm the jitter buffer uses to discard frames in order to
     * adjust the latency.
     *
     * Default: PJMEDIA_JB_DISCARD_PROGRESSIVE
     */
    pjmedia_jb_discard_algo jb_discard_algo;

    /**
     * Enable ICE
     */
    pj_bool_t		enable_ice;

    /**
     * Set the maximum number of host candidates.
     *
     * Default: -1 (maximum not set)
     */
    int			ice_max_host_cands;

    /**
     * ICE session options.
     */
    pj_ice_sess_options	ice_opt;

    /**
     * Disable RTCP component.
     *
     * Default: no
     */
    pj_bool_t		ice_no_rtcp;

    /**
     * Send re-INVITE/UPDATE every after ICE connectivity check regardless
     * the default ICE transport address is changed or not. When this is set
     * to PJ_FALSE, re-INVITE/UPDATE will be sent only when the default ICE
     * transport address is changed.
     *
     * Default: yes
     */
    pj_bool_t		ice_always_update;

    /**
     * Enable TURN relay candidate in ICE.
     */
    pj_bool_t		enable_turn;

    /**
     * Specify TURN domain name or host name, in in "DOMAIN:PORT" or 
     * "HOST:PORT" format.
     */
    pj_str_t		turn_server;

    /**
     * Specify the connection type to be used to the TURN server. Valid
     * values are PJ_TURN_TP_UDP, PJ_TURN_TP_TCP or PJ_TURN_TP_TLS.
     *
     * Default: PJ_TURN_TP_UDP
     */
    pj_turn_tp_type	turn_conn_type;

    /**
     * Specify the credential to authenticate with the TURN server.
     */
    pj_stun_auth_cred	turn_auth_cred;

    /**
     * This specifies TLS settings for TLS transport. It is only be used
     * when this TLS is used to connect to the TURN server.
     */
    pj_turn_sock_tls_cfg turn_tls_setting;

    /**
     * Specify idle time of sound device before it is automatically closed,
     * in seconds. Use value -1 to disable the auto-close feature of sound
     * device
     *
     * Default : 1
     */
    int			snd_auto_close_time;

    /**
     * Specify whether built-in/native preview should be used if available.
     * In some systems, video input devices have built-in capability to show
     * preview window of the device. Using this built-in preview is preferable
     * as it consumes less CPU power. If built-in preview is not available,
     * the library will perform software rendering of the input. If this
     * field is set to PJ_FALSE, software preview will always be used.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t vid_preview_enable_native;

    /**
     * Disable smart media update (ticket #1568). The smart media update
     * will check for any changes in the media properties after a successful
     * SDP negotiation and the media will only be reinitialized when any
     * change is found. When it is disabled, media streams will always be
     * reinitialized after a successful SDP negotiation.
     *
     * Note for third party media, the smart media update requires stream info
     * retrieval capability, see #PJSUA_THIRD_PARTY_STREAM_HAS_GET_INFO.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t no_smart_media_update;

    /**
     * Omit RTCP SDES and BYE in outgoing RTCP packet, this setting will be
     * applied for both audio and video streams. Note that, when RTCP SDES
     * and BYE are set to be omitted, RTCP SDES will still be sent once when
     * the stream starts/stops and RTCP BYE will be sent once when the stream
     * stops.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t no_rtcp_sdes_bye;

    /**
     * Optional callback for audio frame preview right before queued to
     * the speaker.
     * Notes:
     * - application MUST NOT block or perform long operation in the callback
     *   as the callback may be executed in sound device thread
     * - when using software echo cancellation, application MUST NOT modify
     *   the audio data from within the callback, otherwise the echo canceller
     *   will not work properly.
     */
    void (*on_aud_prev_play_frame)(pjmedia_frame *frame);

    /**
     * Optional callback for audio frame preview recorded from the microphone
     * before being processed by any media component such as software echo
     * canceller.
     * Notes:
     * - application MUST NOT block or perform long operation in the callback
     *   as the callback may be executed in sound device thread
     * - when using software echo cancellation, application MUST NOT modify
     *   the audio data from within the callback, otherwise the echo canceller
     *   will not work properly.
     */
    void (*on_aud_prev_rec_frame)(pjmedia_frame *frame);
};


/**
 * Use this function to initialize media config.
 *
 * @param cfg	The media config to be initialized.
 */
PJ_DECL(void) pjsua_media_config_default(pjsua_media_config *cfg);


/**
 * This structure describes codec information, which can be retrieved by
 * calling #pjsua_enum_codecs().
 */
typedef struct pjsua_codec_info
{
    /**
     * Codec unique identification.
     */
    pj_str_t		codec_id;

    /**
     * Codec priority (integer 0-255).
     */
    pj_uint8_t		priority;

    /**
     * Codec description.
     */
    pj_str_t		desc;

    /**
     * Internal buffer.
     */
    char		buf_[64];

} pjsua_codec_info;


/**
 * This structure describes information about a particular media port that
 * has been registered into the conference bridge. Application can query
 * this info by calling #pjsua_conf_get_port_info().
 */
typedef struct pjsua_conf_port_info
{
    /** Conference port number. */
    pjsua_conf_port_id	slot_id;

    /** Port name. */
    pj_str_t		name;

    /** Format. */
    pjmedia_format	format;

    /** Clock rate. */
    unsigned		clock_rate;

    /** Number of channels. */
    unsigned		channel_count;

    /** Samples per frame */
    unsigned		samples_per_frame;

    /** Bits per sample */
    unsigned		bits_per_sample;

    /** Tx level adjustment. */
    float		tx_level_adj;

    /** Rx level adjustment. */
    float		rx_level_adj;

    /** Number of listeners in the array. */
    unsigned		listener_cnt;

    /** Array of listeners (in other words, ports where this port is 
     *  transmitting to).
     */
    pjsua_conf_port_id	listeners[PJSUA_MAX_CONF_PORTS];

} pjsua_conf_port_info;


/**
 * This structure holds information about custom media transport to
 * be registered to pjsua.
 */
typedef struct pjsua_media_transport
{
    /**
     * Media socket information containing the address information
     * of the RTP and RTCP socket.
     */
    pjmedia_sock_info	 skinfo;

    /**
     * The media transport instance.
     */
    pjmedia_transport	*transport;

} pjsua_media_transport;


/**
 * Sound device index constants.
 */
typedef enum pjsua_snd_dev_id
{
    /** 
     * Constant to denote default capture device.
     */
    PJSUA_SND_DEFAULT_CAPTURE_DEV = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV,

    /** 
     * Constant to denote default playback device.
     */
    PJSUA_SND_DEFAULT_PLAYBACK_DEV = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV,

    /**
     * Constant to denote that no sound device is being used.
     */
    PJSUA_SND_NO_DEV = PJMEDIA_AUD_INVALID_DEV,

    /**
     * Constant to denote null sound device.
     */
    PJSUA_SND_NULL_DEV = -99

} pjsua_snd_dev_id;

/**
 * This enumeration specifies the sound device mode.
 */
typedef enum pjsua_snd_dev_mode
{
    /**
     * Open sound device without mic (speaker only).
     */
    PJSUA_SND_DEV_SPEAKER_ONLY = 1,

    /**
     * Do not open sound device, after setting the sound device.
     */
    PJSUA_SND_DEV_NO_IMMEDIATE_OPEN  = 2

} pjsua_snd_dev_mode;


/**
 * This structure specifies the parameters to set the sound device.
 * Use pjsua_snd_dev_param_default() to initialize this structure with
 * default values.
 */
typedef struct pjsua_snd_dev_param
{
    /**
     * Capture dev id.
     *
     * Default: PJMEDIA_AUD_DEFAULT_CAPTURE_DEV
     */
    int			capture_dev;

    /**
     * Playback dev id.
     *
     * Default: PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV
     */
    int			playback_dev;

    /**
     * Sound device mode, refer to #pjsua_snd_dev_mode.
     *
     * Default: 0
     */
    unsigned		mode;

    /*
     * The library will maintain the global sound device settings set when
     * opening the sound device for the first time and later can be modified
     * using #pjsua_snd_set_setting(). These setings are then applied to any
     * sound device when opening. This might be undesirable,
     * e.g: output volume changes when switching sound device due to the
     * use of previously set volume settings.
     *
     * To avoid such case, application can set this to PJ_TRUE and let the
     * sound device use default settings when opening. This will also reset
     * the global sound device settings.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t		use_default_settings;

} pjsua_snd_dev_param;


/**
 * Initialize pjsua_snd_dev_param with default values.
 *
 * @param prm		The parameter.
 */
PJ_DECL(void) pjsua_snd_dev_param_default(pjsua_snd_dev_param *prm);


/**
 * This structure specifies the parameters for conference ports connection.
 * Use pjsua_conf_connect_param_default() to initialize this structure with
 * default values.
 */
typedef struct pjsua_conf_connect_param
{
    /**
     * Signal level adjustment from the source to the sink to make it
     * louder or quieter. Value 1.0 means no level adjustment,
     * while value 0 means to mute the port.
     *
     * Default: 1.0
     */
    float		level;

} pjsua_conf_connect_param;


/**
 * Initialize pjsua_conf_connect_param with default values.
 *
 * @param prm		The parameter.
 */
PJ_DECL(void) pjsua_conf_connect_param_default(pjsua_conf_connect_param *prm);


/**
 * Get maxinum number of conference ports.
 *
 * @return		Maximum number of ports in the conference bridge.
 */
PJ_DECL(unsigned) pjsua_conf_get_max_ports(void);


/**
 * Get current number of active ports in the bridge.
 *
 * @return		The number.
 */
PJ_DECL(unsigned) pjsua_conf_get_active_ports(void);


/**
 * Enumerate all conference ports.
 *
 * @param id		Array of conference port ID to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_conf_ports(pjsua_conf_port_id id[],
					   unsigned *count);


/**
 * Get information about the specified conference port
 *
 * @param port_id	Port identification.
 * @param info		Pointer to store the port info.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_get_port_info( pjsua_conf_port_id port_id,
					       pjsua_conf_port_info *info);


/**
 * Add arbitrary media port to PJSUA's conference bridge. Application
 * can use this function to add the media port that it creates. For
 * media ports that are created by PJSUA-LIB (such as calls, file player,
 * or file recorder), PJSUA-LIB will automatically add the port to
 * the bridge.
 *
 * @param pool		Pool to use.
 * @param port		Media port to be added to the bridge.
 * @param p_id		Optional pointer to receive the conference 
 *			slot id.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_add_port(pj_pool_t *pool,
					 pjmedia_port *port,
					 pjsua_conf_port_id *p_id);


/**
 * Remove arbitrary slot from the conference bridge. Application should only
 * call this function if it registered the port manually with previous call
 * to #pjsua_conf_add_port().
 *
 * @param port_id	The slot id of the port to be removed.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_remove_port(pjsua_conf_port_id port_id);


/**
 * Establish unidirectional media flow from souce to sink. One source
 * may transmit to multiple destinations/sink. And if multiple
 * sources are transmitting to the same sink, the media will be mixed
 * together. Source and sink may refer to the same ID, effectively
 * looping the media.
 *
 * If bidirectional media flow is desired, application needs to call
 * this function twice, with the second one having the arguments
 * reversed.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_connect(pjsua_conf_port_id source,
					pjsua_conf_port_id sink);

/**
 * Establish unidirectional media flow from source to sink. One source
 * may transmit to multiple destinations/sink. And if multiple
 * sources are transmitting to the same sink, the media will be mixed
 * together. Source and sink may refer to the same ID, effectively
 * looping the media.
 *
 * Signal level from the source to the sink can be adjusted by making
 * it louder or quieter via the parameter param. The level adjustment
 * will apply to a specific connection only (i.e. only for the signal
 * from the source to the sink), as compared to
 * pjsua_conf_adjust_tx_level()/pjsua_conf_adjust_rx_level() which
 * applies to all signals from/to that port. The signal adjustment
 * will be cumulative, in this following order:
 * signal from the source will be adjusted with the level specified
 * in pjsua_conf_adjust_rx_level(), then with the level specified
 * via this API, and finally with the level specified to the sink's
 * pjsua_conf_adjust_tx_level().
 *
 * If bidirectional media flow is desired, application needs to call
 * this function twice, with the second one having the arguments
 * reversed.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 * @param prm		Conference port connection param. If set to
 *			NULL, default values will be used.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_connect2(pjsua_conf_port_id source,
					 pjsua_conf_port_id sink,
					 const pjsua_conf_connect_param *prm);


/**
 * Disconnect media flow from the source to destination port.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_disconnect(pjsua_conf_port_id source,
					   pjsua_conf_port_id sink);


/**
 * Adjust the signal level to be transmitted from the bridge to the 
 * specified port by making it louder or quieter.
 *
 * @param slot		The conference bridge slot number.
 * @param level		Signal level adjustment. Value 1.0 means no level
 *			adjustment, while value 0 means to mute the port.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_adjust_tx_level(pjsua_conf_port_id slot,
						float level);

/**
 * Adjust the signal level to be received from the specified port (to
 * the bridge) by making it louder or quieter.
 *
 * @param slot		The conference bridge slot number.
 * @param level		Signal level adjustment. Value 1.0 means no level
 *			adjustment, while value 0 means to mute the port.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_adjust_rx_level(pjsua_conf_port_id slot,
						float level);

/**
 * Get last signal level transmitted to or received from the specified port.
 * The signal level is an integer value in zero to 255, with zero indicates
 * no signal, and 255 indicates the loudest signal level.
 *
 * @param slot		The conference bridge slot number.
 * @param tx_level	Optional argument to receive the level of signal
 *			transmitted to the specified port (i.e. the direction
 *			is from the bridge to the port).
 * @param rx_level	Optional argument to receive the level of signal
 *			received from the port (i.e. the direction is from the
 *			port to the bridge).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_conf_get_signal_level(pjsua_conf_port_id slot,
						 unsigned *tx_level,
						 unsigned *rx_level);


/*****************************************************************************
 * File player and playlist.
 */

/**
 * Create a file player, and automatically add this player to
 * the conference bridge.
 *
 * @param filename	The filename to be played. Currently only
 *			WAV files are supported, and the WAV file MUST be
 *			formatted as 16bit PCM mono/single channel (any
 *			clock rate is supported).
 *			Filename's length must be smaller than PJ_MAXPATH.
 * @param options	Optional option flag. Application may specify
 *			PJMEDIA_FILE_NO_LOOP to prevent playback loop.
 * @param p_id		Pointer to receive player ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_player_create(const pj_str_t *filename,
					 unsigned options,
					 pjsua_player_id *p_id);


/**
 * Create a file playlist media port, and automatically add the port
 * to the conference bridge.
 *
 * @param file_names	Array of file names to be added to the play list.
 *			Note that the files must have the same clock rate,
 *			number of channels, and number of bits per sample.
 *			Each filename's length must be smaller than
 * 			PJ_MAXPATH.
 * @param file_count	Number of files in the array.
 * @param label		Optional label to be set for the media port.
 * @param options	Optional option flag. Application may specify
 *			PJMEDIA_FILE_NO_LOOP to prevent looping.
 * @param p_id		Optional pointer to receive player ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_playlist_create(const pj_str_t file_names[],
					   unsigned file_count,
					   const pj_str_t *label,
					   unsigned options,
					   pjsua_player_id *p_id);

/**
 * Get conference port ID associated with player or playlist.
 *
 * @param id		The file player ID.
 *
 * @return		Conference port ID associated with this player.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_player_id id);


/**
 * Get the media port for the player or playlist.
 *
 * @param id		The player ID.
 * @param p_port	The media port associated with the player.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_player_get_port(pjsua_player_id id,
					   pjmedia_port **p_port);

/**
 * Get additional info about the file player. This operation is not valid
 * for playlist.
 *
 * @param id		The file player ID.
 * @param info		The info.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_player_get_info(pjsua_player_id id,
                                           pjmedia_wav_player_info *info);


/**
 * Get playback position. This operation is not valid for playlist.
 *
 * @param id		The file player ID.
 *
 * @return		The current playback position, in samples. On error,
 * 			return the error code as negative value.
 */
PJ_DECL(pj_ssize_t) pjsua_player_get_pos(pjsua_player_id id);

/**
 * Set playback position. This operation is not valid for playlist.
 *
 * @param id		The file player ID.
 * @param samples	The playback position, in samples. Application can
 *			specify zero to re-start the playback.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_player_set_pos(pjsua_player_id id,
					  pj_uint32_t samples);

/**
 * Close the file of playlist, remove the player from the bridge, and free
 * resources associated with the file player or playlist.
 *
 * @param id		The file player ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_player_destroy(pjsua_player_id id);


/*****************************************************************************
 * File recorder.
 */

/**
 * Create a file recorder, and automatically connect this recorder to
 * the conference bridge. The recorder currently supports recording WAV file.
 * The type of the recorder to use is determined by the extension of the file 
 * (e.g. ".wav").
 *
 * @param filename	Output file name. The function will determine the
 *			default format to be used based on the file extension.
 *			Currently ".wav" is supported on all platforms.
 *			Filename's length must be smaller than PJ_MAXPATH.
 * @param enc_type	Optionally specify the type of encoder to be used to
 *			compress the media, if the file can support different
 *			encodings. This value must be zero for now.
 * @param enc_param	Optionally specify codec specific parameter to be 
 *			passed to the file writer. 
 *			For .WAV recorder, this value must be NULL.
 * @param max_size	Maximum file size. Specify zero or -1 to remove size
 *			limitation. This value must be zero or -1 for now.
 * @param options	Optional options.
 * @param p_id		Pointer to receive the recorder instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_recorder_create(const pj_str_t *filename,
					   unsigned enc_type,
					   void *enc_param,
					   pj_ssize_t max_size,
					   unsigned options,
					   pjsua_recorder_id *p_id);


/**
 * Get conference port associated with recorder.
 *
 * @param id		The recorder ID.
 *
 * @return		Conference port ID associated with this recorder.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_recorder_get_conf_port(pjsua_recorder_id id);


/**
 * Get the media port for the recorder.
 *
 * @param id		The recorder ID.
 * @param p_port	The media port associated with the recorder.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_recorder_get_port(pjsua_recorder_id id,
					     pjmedia_port **p_port);


/**
 * Destroy recorder (this will complete recording).
 *
 * @param id		The recorder ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id);


/*****************************************************************************
 * Sound devices.
 */

/**
 * Enum all audio devices installed in the system.
 *
 * @param info		Array of info to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_aud_devs(pjmedia_aud_dev_info info[],
					 unsigned *count);

/**
 * Enum all sound devices installed in the system (old API).
 *
 * @param info		Array of info to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_snd_devs(pjmedia_snd_dev_info info[],
					 unsigned *count);

/**
 * Get currently active sound devices. If sound devices has not been created
 * (for example when pjsua_start() is not called), it is possible that
 * the function returns PJ_SUCCESS with -1 as device IDs.
 * See also #pjsua_snd_dev_id constants.
 *
 * @param capture_dev   On return it will be filled with device ID of the 
 *			capture device.
 * @param playback_dev	On return it will be filled with device ID of the 
 *			device ID of the playback device.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_get_snd_dev(int *capture_dev,
				       int *playback_dev);


/**
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 *
 * Note that this function will always try to open the sound device
 * immediately. If immediate open is not preferred, use pjsua_set_snd_dev2()
 * with PJSUA_SND_DEV_NO_IMMEDIATE_OPEN flag.
 *
 * @param capture_dev   Device ID of the capture device.
 * @param playback_dev	Device ID of the playback device.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_set_snd_dev(int capture_dev,
				       int playback_dev);

/**
 * Get sound device parameters such as playback & capture device IDs and mode.
 *
 * @param snd_param	On return, it is set with sound device param.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_get_snd_dev2(pjsua_snd_dev_param *snd_param);


/**
 * Select or change sound device according to the specified param.
 *
 * @param snd_param	Sound device param. 
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_set_snd_dev2(const pjsua_snd_dev_param *snd_param);


/**
 * Set pjsua to use null sound device. The null sound device only provides
 * the timing needed by the conference bridge, and will not interract with
 * any hardware.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_set_null_snd_dev(void);


/**
 * Disconnect the main conference bridge from any sound devices, and let
 * application connect the bridge to it's own sound device/master port.
 *
 * @return		The port interface of the conference bridge, 
 *			so that application can connect this to it's own
 *			sound device or master port.
 */
PJ_DECL(pjmedia_port*) pjsua_set_no_snd_dev(void);


/**
 * Change the echo cancellation settings.
 *
 * The behavior of this function depends on whether the sound device is
 * currently active, and if it is, whether device or software AEC is 
 * being used. 
 *
 * If the sound device is currently active, and if the device supports AEC,
 * this function will forward the change request to the device and it will
 * be up to the device on whether support the request. If software AEC is
 * being used (the software EC will be used if the device does not support
 * AEC), this function will change the software EC settings. In all cases,
 * the setting will be saved for future opening of the sound device.
 *
 * If the sound device is not currently active, this will only change the
 * default AEC settings and the setting will be applied next time the 
 * sound device is opened.
 *
 * @param tail_ms	The tail length, in miliseconds. Set to zero to
 *			disable AEC.
 * @param options	Options to be passed to pjmedia_echo_create().
 *			Normally the value should be zero.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_set_ec(unsigned tail_ms, unsigned options);


/**
 * Get current echo canceller tail length. 
 *
 * @param p_tail_ms	Pointer to receive the tail length, in miliseconds. 
 *			If AEC is disabled, the value will be zero.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_get_ec_tail(unsigned *p_tail_ms);


/**
 * Get echo canceller statistics.
 *
 * @param p_stat	    Pointer to receive the stat.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjsua_get_ec_stat(pjmedia_echo_stat *p_stat);


/**
 * Check whether the sound device is currently active. The sound device
 * may be inactive if the application has set the auto close feature to
 * non-zero (the snd_auto_close_time setting in #pjsua_media_config), or
 * if null sound device or no sound device has been configured via the
 * #pjsua_set_no_snd_dev() function.
 */
PJ_DECL(pj_bool_t) pjsua_snd_is_active(void);

    
/**
 * Configure sound device setting to the sound device being used. If sound 
 * device is currently active, the function will forward the setting to the
 * sound device instance to be applied immediately, if it supports it. 
 *
 * The setting will be saved for future opening of the sound device, if the 
 * "keep" argument is set to non-zero. If the sound device is currently
 * inactive, and the "keep" argument is false, this function will return
 * error.
 * 
 * Note that in case the setting is kept for future use, it will be applied
 * to any devices, even when application has changed the sound device to be
 * used. To reset the setting, application can call #pjsua_set_snd_dev2()
 * with \a use_default_settings set to PJ_TRUE.
 *
 * Note also that the echo cancellation setting should be set with 
 * #pjsua_set_ec() API instead.
 *
 * See also #pjmedia_aud_stream_set_cap() for more information about setting
 * an audio device capability.
 *
 * @param cap		The sound device setting to change.
 * @param pval		Pointer to value. Please see #pjmedia_aud_dev_cap
 *			documentation about the type of value to be 
 *			supplied for each setting.
 * @param keep		Specify whether the setting is to be kept for future
 *			use.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_snd_set_setting(pjmedia_aud_dev_cap cap,
					   const void *pval,
					   pj_bool_t keep);

/**
 * Retrieve a sound device setting. If sound device is currently active,
 * the function will forward the request to the sound device. If sound device
 * is currently inactive, and if application had previously set the setting
 * and mark the setting as kept, then that setting will be returned.
 * Otherwise, this function will return error.
 *
 * Note that echo cancellation settings should be retrieved with 
 * #pjsua_get_ec_tail() API instead.
 *
 * @param cap		The sound device setting to retrieve.
 * @param pval		Pointer to receive the value. 
 *			Please see #pjmedia_aud_dev_cap documentation about
 *			the type of value to be supplied for each setting.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_snd_get_setting(pjmedia_aud_dev_cap cap,
					   void *pval);


/**
 * Opaque type of extra sound device, an additional sound device
 * beside the primary sound device (the one instantiated via
 * pjsua_set_snd_dev() or pjsua_set_snd_dev2()). This sound device is
 * also registered to conference bridge so it can be used as a normal
 * conference bridge port, e.g: connect it to/from other ports,
 * adjust/check audio level, etc. The conference bridge port ID can be
 * queried using pjsua_ext_snd_dev_get_conf_port().
 *
 * Application may also use this API to improve media clock. Normally
 * media clock is driven by sound device in master port, but unfortunately
 * some sound devices may produce jittery clock. To improve media clock,
 * application can install Null Sound Device (i.e: using
 * pjsua_set_null_snd_dev()), which will act as a master port, and instantiate
 * the sound device as extra sound device.
 *
 * Note that extra sound device will not have auto-close upon idle feature.
 * Also note that currently extra sound device only supports mono channel.
 */
typedef struct pjsua_ext_snd_dev pjsua_ext_snd_dev;


/**
 * Create an extra sound device and register it to conference bridge.
 *
 * @param param	Sound device port param. Currently this only supports
 *			mono channel, so channel count must be set to 1.
 * @param p_snd		The extra sound device instance.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_ext_snd_dev_create(pjmedia_snd_port_param *param,
					      pjsua_ext_snd_dev **p_snd);


/**
 * Destroy an extra sound device and unregister it from conference bridge.
 *
 * @param snd		The extra sound device instance.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_ext_snd_dev_destroy(pjsua_ext_snd_dev *snd);


/**
 * Get sound port instance of an extra sound device.
 *
 * @param snd		The extra sound device instance.
 *
 * @return		The sound port instance.
 */
PJ_DECL(pjmedia_snd_port*) pjsua_ext_snd_dev_get_snd_port(
					    pjsua_ext_snd_dev *snd);

/**
 * Get conference port ID of an extra sound device.
 *
 * @param snd		The extra sound device instance.
 *
 * @return		The conference port ID.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_ext_snd_dev_get_conf_port(
					    pjsua_ext_snd_dev *snd);


/*****************************************************************************
 * Codecs.
 */

/**
 * Enum all supported codecs in the system.
 *
 * @param id		Array of ID to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_enum_codecs( pjsua_codec_info id[],
				        unsigned *count );


/**
 * Change codec priority.
 *
 * @param codec_id	Codec ID, which is a string that uniquely identify
 *			the codec (such as "speex/8000"). Please see pjsua
 *			manual or pjmedia codec reference for details.
 * @param priority	Codec priority, 0-255, where zero means to disable
 *			the codec.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_set_priority( const pj_str_t *codec_id,
					       pj_uint8_t priority );


/**
 * Get codec parameters.
 *
 * @param codec_id	Codec ID.
 * @param param		Structure to receive codec parameters.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_get_param( const pj_str_t *codec_id,
					    pjmedia_codec_param *param );


/**
 * Set codec parameters.
 *
 * @param codec_id	Codec ID.
 * @param param		Codec parameter to set. Set to NULL to reset
 *			codec parameter to library default settings.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_set_param( const pj_str_t *codec_id,
					    const pjmedia_codec_param *param);


#if DISABLED_FOR_TICKET_1185
/**
 * Create UDP media transports for all the calls. This function creates
 * one UDP media transport for each call.
 *
 * @param cfg		Media transport configuration. The "port" field in the
 *			configuration is used as the start port to bind the
 *			sockets.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjsua_media_transports_create(const pjsua_transport_config *cfg);


/**
 * Register custom media transports to be used by calls. There must
 * enough media transports for all calls.
 *
 * @param tp		The media transport array.
 * @param count		Number of elements in the array. This number MUST
 *			match the number of maximum calls configured when
 *			pjsua is created.
 * @param auto_delete	Flag to indicate whether the transports should be
 *			destroyed when pjsua is shutdown.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjsua_media_transports_attach( pjsua_media_transport tp[],
			       unsigned count,
			       pj_bool_t auto_delete);
#endif


/* end of MEDIA API */
/**
 * @}
 */


/*****************************************************************************
 * VIDEO API
 */


/**
 * @defgroup PJSUA_LIB_VIDEO PJSUA-API Video
 * @ingroup PJSUA_LIB
 * @brief Video support
 * @{
 */

/*
 * Video devices API
 */


/**
 * Controls whether PJSUA-LIB should not initialize video device subsystem
 * in the PJSUA initialization. The video device subsystem initialization
 * may need to open cameras to enumerates available cameras and their
 * capabilities, which may not be preferable for some applications because
 * it may trigger privacy-alert/permission notification on application startup
 * (e.g: on Android app).
 *
 * If this is set, later application should manually initialize video device
 * subsystem when it needs to use any video devices (camera and renderer),
 * i.e: by invoking pjmedia_vid_dev_subsys_init() for PJSUA or
 * VidDevManager::initSubsys() for PJSUA2.
 *
 * Note that pjmedia_vid_dev_subsys_init() should not be called multiple
 * times (unless each has corresponding pjmedia_vid_dev_subsys_shutdown()),
 * while VidDevManager::initSubsys() is safe to be called multiple times.
 *
 * Default: 0 (no)
 */
#ifndef PJSUA_DONT_INIT_VID_DEV_SUBSYS
#   define PJSUA_DONT_INIT_VID_DEV_SUBSYS	0
#endif


/**
 * Get the number of video devices installed in the system.
 *
 * @return		The number of devices.
 */
PJ_DECL(unsigned) pjsua_vid_dev_count(void);

/**
 * Retrieve the video device info for the specified device index.
 *
 * @param id		The device index.
 * @param vdi		Device info to be initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_dev_get_info(pjmedia_vid_dev_index id,
                                            pjmedia_vid_dev_info *vdi);

/**
 * Check whether the video capture device is currently active, i.e. if
 * a video preview has been started or there is a video call using
 * the device. This function will return PJ_FALSE for video renderer device.
 *
 * @param id		The video device index.
 *
 * @return		PJ_TRUE if active, PJ_FALSE otherwise.
 */
PJ_DECL(pj_bool_t) pjsua_vid_dev_is_active(pjmedia_vid_dev_index id);

/**
 * Configure the capability of a video capture device. If the device is 
 * currently active (i.e. if there is a video call using the device or
 * a video preview has been started), the function will forward the setting
 * to the video device instance to be applied immediately, if it supports it.
 *
 * The setting will be saved for future opening of the video device, if the 
 * "keep" argument is set to non-zero. If the video device is currently
 * inactive, and the "keep" argument is false, this function will return
 * error.
 *
 * Note: This function will only works for video capture devices. To
 * configure the setting of video renderer device instances, use
 * pjsua_vid_win API instead.
 *
 * Warning: If application refreshes the video device list, it needs to
 * manually update the settings to reflect the newly updated video device
 * indexes. See #pjmedia_vid_dev_refresh() for more information.
 *
 * See also pjmedia_vid_stream_set_cap() for more information about setting
 * a video device capability.
 *
 * @param id		The video device index.
 * @param cap		The video device capability to change.
 * @param pval		Pointer to value. Please see #pjmedia_vid_dev_cap
 *			documentation about the type of value to be 
 *			supplied for each setting.
 * @param keep          (see description)
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_dev_set_setting(pjmedia_vid_dev_index id,
					       pjmedia_vid_dev_cap cap,
					       const void *pval,
					       pj_bool_t keep);

/**
 * Retrieve the value of a video capture device setting. If the device is
 * currently active (i.e. if there is a video call using the device or
 * a video preview has been started), the function will forward the request
 * to the video device. If video device is currently inactive, and if
 * application had previously set the setting and mark the setting as kept,
 * then that setting will be returned. Otherwise, this function will return
 * error.
 * The function only works for video capture device.
 *
 * @param id		The video device index.
 * @param cap		The video device capability to retrieve.
 * @param pval		Pointer to receive the value. 
 *			Please see #pjmedia_vid_dev_cap documentation about
 *			the type of value to be supplied for each setting.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_dev_get_setting(pjmedia_vid_dev_index id,
					       pjmedia_vid_dev_cap cap,
					       void *pval);

/**
 * Enum all video devices installed in the system.
 *
 * @param info		Array of info to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_enum_devs(pjmedia_vid_dev_info info[],
					 unsigned *count);


/*
 * Video preview API
 */

/**
 * Parameters for starting video preview with pjsua_vid_preview_start().
 * Application should initialize this structure with
 * pjsua_vid_preview_param_default().
 */
typedef struct pjsua_vid_preview_param
{
    /**
     * Device ID for the video renderer to be used for rendering the
     * capture stream for preview. This parameter is ignored if native
     * preview is being used.
     *
     * Default: PJMEDIA_VID_DEFAULT_RENDER_DEV
     */
    pjmedia_vid_dev_index	rend_id;

    /**
     * Show window initially.
     *
     * Default: PJ_TRUE.
     */
    pj_bool_t			show;

    /**
     * Window flags.  The value is a bitmask combination of
     * #pjmedia_vid_dev_wnd_flag.
     *
     * Default: 0.
     */
    unsigned			wnd_flags;
    
    /**
     * Media format. Initialize this with #pjmedia_format_init_video().
     * If left unitialized, this parameter will not be used.
     */
    pjmedia_format              format;
    
    /**
     * Optional output window to be used to display the video preview.
     * This parameter will only be used if the video device supports
     * PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW capability and the capability
     * is not read-only.
     */    
    pjmedia_vid_dev_hwnd	wnd;

} pjsua_vid_preview_param;


/**
 * Initialize pjsua_vid_preview_param
 *
 * @param p		The parameter to be initialized.
 */
PJ_DECL(void) pjsua_vid_preview_param_default(pjsua_vid_preview_param *p);

/**
 * Determine if the specified video input device has built-in native
 * preview capability. This is a convenience function that is equal to
 * querying device's capability for PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW
 * capability.
 *
 * @param id		The capture device ID.
 *
 * @return		PJ_TRUE if it has.
 */
PJ_DECL(pj_bool_t) pjsua_vid_preview_has_native(pjmedia_vid_dev_index id);

/**
 * Start video preview window for the specified capture device.
 *
 * @param id		The capture device ID where its preview will be
 * 			started.
 * @param p		Optional video preview parameters. Specify NULL
 * 			to use default values.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_preview_start(pjmedia_vid_dev_index id,
                                             const pjsua_vid_preview_param *p);

/**
 * Get the preview window handle associated with the capture device, if any.
 *
 * @param id		The capture device ID.
 *
 * @return		The window ID of the preview window for the
 * 			specified capture device ID, or PJSUA_INVALID_ID if
 * 			preview has not been started for the device.
 */
PJ_DECL(pjsua_vid_win_id) pjsua_vid_preview_get_win(pjmedia_vid_dev_index id);

/**
 * Get video conference slot ID of the specified capture device, if any.
 *
 * @param id		The capture device ID.
 *
 * @return		The video conference slot ID of the specified capture
 *			device ID, or PJSUA_INVALID_ID if preview has not been
 *			started for the device.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_vid_preview_get_vid_conf_port(
						    pjmedia_vid_dev_index id);

/**
 * Stop video preview.
 *
 * @param id		The capture device ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_preview_stop(pjmedia_vid_dev_index id);


/*
 * Video window manipulation API.
 */

/**
 * This structure describes video window info.
 */
typedef struct pjsua_vid_win_info
{
    /**
     * Flag to indicate whether this window is a native window,
     * such as created by built-in preview device. If this field is
     * PJ_TRUE, only the native window handle field of this
     * structure is valid.
     */
    pj_bool_t is_native;

    /**
     * Native window handle.
     */
    pjmedia_vid_dev_hwnd hwnd;

    /**
     * Renderer device ID.
     */
    pjmedia_vid_dev_index rdr_dev;

    /**
     * Renderer port ID in the video conference bridge.
     */
    pjsua_conf_port_id slot_id;

    /**
     * Window show status. The window is hidden if false.
     */
    pj_bool_t	show;

    /**
     * Window position.
     */
    pjmedia_coord pos;

    /**
     * Window size.
     */
    pjmedia_rect_size size;

} pjsua_vid_win_info;


/**
 * Enumerates all video windows.
 *
 * @param wids		Array of window ID to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_enum_wins(pjsua_vid_win_id wids[],
					 unsigned *count);


/**
 * Get window info.
 *
 * @param wid		The video window ID.
 * @param wi		The video window info to be initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_get_info(pjsua_vid_win_id wid,
                                            pjsua_vid_win_info *wi);

/**
 * Show or hide window. This operation is not valid for native windows
 * (pjsua_vid_win_info.is_native=PJ_TRUE), on which native windowing API
 * must be used instead.
 *
 * @param wid		The video window ID.
 * @param show		Set to PJ_TRUE to show the window, PJ_FALSE to
 * 			hide the window.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_set_show(pjsua_vid_win_id wid,
                                            pj_bool_t show);

/**
 * Set video window position. This operation is not valid for native windows
 * (pjsua_vid_win_info.is_native=PJ_TRUE), on which native windowing API
 * must be used instead.
 *
 * @param wid		The video window ID.
 * @param pos		The window position.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_set_pos(pjsua_vid_win_id wid,
                                           const pjmedia_coord *pos);

/**
 * Resize window. This operation is not valid for native windows
 * (pjsua_vid_win_info.is_native=PJ_TRUE), on which native windowing API
 * must be used instead.
 *
 * @param wid		The video window ID.
 * @param size		The new window size.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_set_size(pjsua_vid_win_id wid,
                                            const pjmedia_rect_size *size);

/**
 * Set output window. This operation is valid only when the underlying
 * video device supports PJMEDIA_VIDEO_DEV_CAP_OUTPUT_WINDOW capability AND
 * allows the output window to be changed on-the-fly. Currently it is only
 * supported on Android.
 *
 * @param wid		The video window ID.
 * @param win		The new output window.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_set_win(pjsua_vid_win_id wid,
                                           const pjmedia_vid_dev_hwnd *win);

/**
 * Rotate the video window. This function will change the video orientation
 * and also possibly the video window size (width and height get swapped).
 * This operation is not valid for native windows (pjsua_vid_win_info.is_native
 * =PJ_TRUE), on which native windowing API must be used instead.
 *
 * @param wid		The video window ID.
 * @param angle		The rotation angle in degrees, must be multiple of 90.
 *			Specify positive value for clockwise rotation or
 *			negative value for counter-clockwise rotation.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_rotate(pjsua_vid_win_id wid,
					  int angle);


/**
 * Set video window full-screen. This operation is valid only when the
 * underlying video device supports PJMEDIA_VID_DEV_CAP_OUTPUT_FULLSCREEN
 * capability. Currently it is only supported on SDL backend.
 *
 * @param wid		The video window ID.
 * @param mode   	Fullscreen mode, see pjmedia_vid_dev_fullscreen_flag.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_win_set_fullscreen(
					pjsua_vid_win_id wid,
					pjmedia_vid_dev_fullscreen_flag mode);


/*
 * Video codecs API
 */

/**
 * Enum all supported video codecs in the system.
 *
 * @param id		Array of ID to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_enum_codecs( pjsua_codec_info id[],
					    unsigned *count );


/**
 * Change video codec priority.
 *
 * @param codec_id	Codec ID, which is a string that uniquely identify
 *			the codec (such as "H263/90000"). Please see pjsua
 *			manual or pjmedia codec reference for details.
 * @param priority	Codec priority, 0-255, where zero means to disable
 *			the codec.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_codec_set_priority( const pj_str_t *codec_id,
						   pj_uint8_t priority );


/**
 * Get video codec parameters.
 *
 * @param codec_id	Codec ID.
 * @param param		Structure to receive video codec parameters.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_codec_get_param(
					const pj_str_t *codec_id,
					pjmedia_vid_codec_param *param);


/**
 * Set video codec parameters.
 *
 * @param codec_id	Codec ID.
 * @param param		Codec parameter to set. Set to NULL to reset
 *			codec parameter to library default settings.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_codec_set_param( 
					const pj_str_t *codec_id,
					const pjmedia_vid_codec_param *param);


/*
 * Video conference API
 */

/**
 * This structure describes information about a particular video media port
 * that has been registered into the video conference bridge. Application
 * can query this info by calling #pjsua_vid_conf_get_port_info().
 */
typedef struct pjsua_vid_conf_port_info
{
    /** Conference port number. */
    pjsua_conf_port_id	slot_id;

    /** Port name. */
    pj_str_t		name;

    /** Format. */
    pjmedia_format	format;

    /** Number of listeners in the array. */
    unsigned		listener_cnt;

    /** Array of listeners (in other words, ports where this port is 
     *  transmitting to).
     */
    pjsua_conf_port_id	listeners[PJSUA_MAX_CONF_PORTS];

    /** Number of transmitters in the array. */
    unsigned		transmitter_cnt;

    /** Array of transmitters (in other words, ports where this port is 
     *  receiving from).
     */
    pjsua_conf_port_id	transmitters[PJSUA_MAX_CONF_PORTS];

} pjsua_vid_conf_port_info;


/**
 * Get current number of active ports in the bridge.
 *
 * @return		The number.
 */
PJ_DECL(unsigned) pjsua_vid_conf_get_active_ports(void);


/**
 * Enumerate all video conference ports.
 *
 * @param id		Array of conference port ID to be initialized.
 * @param count		On input, specifies max elements in the array.
 *			On return, it contains actual number of elements
 *			that have been initialized.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_enum_ports(pjsua_conf_port_id id[],
					       unsigned *count);


/**
 * Get information about the specified video conference port
 *
 * @param port_id	Port identification.
 * @param info		Pointer to store the port info.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_get_port_info(
					    pjsua_conf_port_id port_id,
					    pjsua_vid_conf_port_info *info);


/**
 * Add arbitrary video media port to PJSUA's video conference bridge.
 * Application can use this function to add the media port that it creates.
 * For media ports that are created by PJSUA-LIB (such as calls, AVI player),
 * PJSUA-LIB will automatically add the port to the bridge.
 *
 * @param pool		Pool to use.
 * @param port		Media port to be added to the bridge.
 * @param param		Currently this is not used and must be set to NULL.
 * @param p_id		Optional pointer to receive the conference 
 *			slot id.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_add_port(pj_pool_t *pool,
					     pjmedia_port *port,
					     const void *param,
					     pjsua_conf_port_id *p_id);


/**
 * Remove arbitrary slot from the video conference bridge. Application should
 * only call this function if it registered the port manually with previous
 * call to #pjsua_vid_conf_add_port().
 *
 * @param port_id	The slot id of the port to be removed.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_remove_port(pjsua_conf_port_id port_id);


/**
 * Establish unidirectional video flow from souce to sink. One source
 * may transmit to multiple destinations/sink. And if multiple
 * sources are transmitting to the same sink, the video will be mixed
 * together (currently, each source will be resized down so all sources will
 * occupy the same portion in the sink video frame). Source and sink may
 * refer to the same ID, effectively looping the media.
 *
 * If bidirectional media flow is desired, application needs to call
 * this function twice, with the second one having the arguments
 * reversed.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 * @param param		Currently this is not used and must be set to NULL.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_connect(pjsua_conf_port_id source,
					    pjsua_conf_port_id sink,
					    const void *param);


/**
 * Disconnect video flow from the source to destination port.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_disconnect(pjsua_conf_port_id source,
					       pjsua_conf_port_id sink);


/**
 * Update or refresh port states from video port info. Some port may
 * change its port info in the middle of a session, for example when
 * a video stream decoder learns that incoming video size or frame rate
 * has changed, video conference needs to be informed to update its
 * internal states.
 *
 * @param port_id	The slot id of the port to be updated.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) pjsua_vid_conf_update_port(pjsua_conf_port_id port_id);


/* end of VIDEO API */
/**
 * @}
 */


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSUA_H__ */

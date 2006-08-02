/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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

/* Include all PJSIP-UA headers */
#include <pjsip_ua.h>

/* Include all PJSIP-SIMPLE headers */
#include <pjsip_simple.h>

/* Include all PJLIB-UTIL headers. */
#include <pjlib-util.h>

/* Include all PJLIB headers. */
#include <pjlib.h>


/**
 * @defgroup PJSUA_LIB PJSUA API
 * @ingroup PJSIP
 * @brief Very high level API for constructing SIP UA applications.
 * @{
 *
 * PJSUA API is very high level API for constructing SIP user agent
 * applications. It wraps together the signaling and media functionalities
 * into an easy to use call API, provides account management, buddy
 * management, presence, instant messaging, along with multimedia
 * features such as conferencing, file streaming, local playback,
 * voice recording, and so on.
 *
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
 * so application must also link with these libraries as well.
 *
 * @section root_using_pjsua_lib Using PJSUA API
 *
 * Please refer to @ref using_pjsua_lib on how to use PJSUA API.
 */ 

PJ_BEGIN_DECL


/** Forward declaration */
typedef struct pjsua_media_config pjsua_media_config;


/*****************************************************************************
 * BASE API
 */

/**
 * @defgroup PJSUA_LIB_BASE Base API
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
 * Before anything else, application must create PJSUA by calling #pjsua_create().
 * This, among other things, will initialize PJLIB, which is crucial before 
 * any PJLIB functions can be called.
 *
 * @subsection init_pjsua_lib Initializing PJSUA
 *
 * After PJSUA is created, application can initialize PJSUA by calling
 * #pjsua_init(). This function takes several configuration settings in the
 * argument, so application normally would want to set these configuration
 * before passing them to #pjsua_init().
 *
 * Sample code to initialize PJSUA:
 \code

    pjsua_config	 ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config   media_cfg;

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
    ..

 \endcode
 *
 * @subsection other_init_pjsua_lib Other Initialization
 *
 * After PJSUA is initialized with #pjsua_init(), application will normally
 * need/want to perform the following tasks:
 *
 *  - create SIP transport with #pjsua_transport_create(). Please see
 *    @ref PJSUA_LIB_TRANSPORT section for more info.
 *  - create one or more SIP accounts with #pjsua_acc_add() or
 *    #pjsua_acc_add_local(). Please see @ref PJSUA_LIB_ACC for more info.
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
 * are properly configured, and apply default settings when it's not, or
 * report error status when it is unable to recover from missing setting.
 *
 * Most settings can be changed during run-time. For example, application
 * may add, modify, or delete accounts, buddies, or change media settings
 * during run-time.
 */

/** Constant to identify invalid ID for all sorts of IDs. */
#define PJSUA_INVALID_ID	    (-1)

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



/**
 * Maximum proxies in account.
 */
#ifndef PJSUA_ACC_MAX_PROXIES
#   define PJSUA_ACC_MAX_PROXIES    8
#endif



/**
 * Logging configuration.
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
     * Optional callback function to be called to write log to 
     * application specific device. This function will be called for
     * log messages on input verbosity level.
     */
    void       (*cb)(int level, const char *data, pj_size_t len);


} pjsua_logging_config;


/**
 * Use this function to initialize logging config.
 *
 * @param cfg	The logging config to be initialized.
 */
PJ_INLINE(void) pjsua_logging_config_default(pjsua_logging_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->msg_logging = PJ_TRUE;
    cfg->level = 5;
    cfg->console_level = 4;
    cfg->decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		 PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE;
}

/**
 * Use this function to duplicate logging config.
 *
 * @param pool	    Pool to use.
 * @param dst	    Destination config.
 * @param src	    Source config.
 */
PJ_INLINE(void) pjsua_logging_config_dup(pj_pool_t *pool,
					 pjsua_logging_config *dst,
					 const pjsua_logging_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup_with_null(pool, &dst->log_filename, &src->log_filename);
}



/**
 * Application callbacks.
 */
typedef struct pjsua_callback
{
    /**
     * Notify application when invite state has changed.
     * Application may then query the call info to get the
     * detail call states.
     */
    void (*on_call_state)(pjsua_call_id call_id, pjsip_event *e);

    /**
     * Notify application on incoming call.
     */
    void (*on_incoming_call)(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata);

    /**
     * Notify application when media state in the call has changed.
     * Normal application would need to implement this callback, e.g.
     * to connect the call's media to sound device.
     */
    void (*on_call_media_state)(pjsua_call_id call_id);

    /**
     * Notify application on call being transfered.
     * Application can decide to accept/reject transfer request
     * by setting the code (default is 200). When this callback
     * is not defined, the default behavior is to accept the
     * transfer.
     */
    void (*on_call_transfered)(pjsua_call_id call_id,
			       const pj_str_t *dst,
			       pjsip_status_code *code);

    /**
     * Notify application when registration status has changed.
     * Application may then query the account info to get the
     * registration details.
     */
    void (*on_reg_state)(pjsua_acc_id acc_id);

    /**
     * Notify application when the buddy state has changed.
     * Application may then query the buddy into to get the details.
     */
    void (*on_buddy_state)(pjsua_buddy_id buddy_id);

    /**
     * Notify application on incoming pager (i.e. MESSAGE request).
     * Argument call_id will be -1 if MESSAGE request is not related to an
     * existing call.
     */
    void (*on_pager)(pjsua_call_id call_id, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *body);

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
     */
    void (*on_pager_status)(pjsua_call_id call_id,
			    const pj_str_t *to,
			    const pj_str_t *body,
			    void *user_data,
			    pjsip_status_code status,
			    const pj_str_t *reason);

    /**
     * Notify application about typing indication.
     */
    void (*on_typing)(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      pj_bool_t is_typing);

} pjsua_callback;




/**
 * PJSUA settings.
 */
typedef struct pjsua_config
{

    /** 
     * Maximum calls to support (default: 4) 
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
     * Number of outbound proxies in the array.
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
     * Number of credentials in the credential array.
     */
    unsigned	    cred_count;

    /** 
     * Array of credentials. These credentials will be used by all accounts,
     * and can be used to authenticate against outbound proxies.
     */
    pjsip_cred_info cred_info[PJSUA_ACC_MAX_PROXIES];

    /**
     * Application callback.
     */
    pjsua_callback  cb;

    /**
     * User agent string (default empty)
     */
    pj_str_t	    user_agent;

} pjsua_config;


/**
 * Use this function to initialize pjsua config.
 *
 * @param cfg	pjsua config to be initialized.
 */
PJ_INLINE(void) pjsua_config_default(pjsua_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->max_calls = 4;
    cfg->thread_cnt = 1;
}


/**
 * Duplicate credential.
 */
PJ_INLINE(void) pjsip_cred_dup( pj_pool_t *pool,
				pjsip_cred_info *dst,
				const pjsip_cred_info *src)
{
    pj_strdup_with_null(pool, &dst->realm, &src->realm);
    pj_strdup_with_null(pool, &dst->scheme, &src->scheme);
    pj_strdup_with_null(pool, &dst->username, &src->username);
    pj_strdup_with_null(pool, &dst->data, &src->data);

}


/**
 * Duplicate pjsua_config.
 */
PJ_INLINE(void) pjsua_config_dup(pj_pool_t *pool,
				 pjsua_config *dst,
				 const pjsua_config *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(*src));

    for (i=0; i<src->outbound_proxy_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->outbound_proxy[i],
			    &src->outbound_proxy[i]);
    }

    for (i=0; i<src->cred_count; ++i) {
	pjsip_cred_dup(pool, &dst->cred_info[i], &src->cred_info[i]);
    }

    pj_strdup_with_null(pool, &dst->user_agent, &src->user_agent);
}



/**
 * This structure describes additional information to be sent with
 * outgoing SIP message.
 */
typedef struct pjsua_msg_data
{
    /**
     * Additional message headers as linked list.
     */
    pjsip_hdr	hdr_list;

    /**
     * MIME type of optional message body. 
     */
    pj_str_t	content_type;

    /**
     * Optional message body.
     */
    pj_str_t	msg_body;

} pjsua_msg_data;


/**
 * Initialize message data.
 *
 * @param msg_data  Message data to be initialized.
 */
PJ_INLINE(void) pjsua_msg_data_init(pjsua_msg_data *msg_data)
{
    pj_bzero(msg_data, sizeof(*msg_data));
    pj_list_init(&msg_data->hdr_list);
}



/**
 * Instantiate pjsua application. Application must call this function before
 * calling any other functions, to make sure that the underlying libraries
 * are properly initialized. Once this function has returned success,
 * application must call pjsua_destroy() before quitting.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_create(void);


/**
 * Initialize pjsua with the specified settings. All the settings are 
 * optional, and the default values will be used when the config is not
 * specified.
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
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_start(void);


/**
 * Destroy pjsua. This function must be called once PJSUA is created. To
 * make it easier for application, application may call this function
 * several times with no danger.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_destroy(void);


/**
 * Poll pjsua for events, and if necessary block the caller thread for
 * the specified maximum interval (in miliseconds).
 *
 * @param msec_timeout	Maximum time to wait, in miliseconds.
 *
 * @return  The number of events that have been handled during the
 *	    poll. Negative value indicates error, and application
 *	    can retrieve the error as (err = -return_value).
 */
PJ_DECL(int) pjsua_handle_events(unsigned msec_timeout);


/**
 * Create memory pool.
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
 * Probably is only valid after #pjsua_init() is called.
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


/*****************************************************************************
 * Utilities.
 *
 */

/**
 * Verify that valid SIP url is given.
 *
 * @param c_url		The URL, as NULL terminated string.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_verify_sip_url(const char *c_url);


/**
 * Display error message for the specified error code.
 *
 * @param sender	The log sender field.
 * @param title		Message title for the error.
 * @param status	Status code.
 */
PJ_DECL(void) pjsua_perror(const char *sender, const char *title, 
			   pj_status_t status);




/**
 * @}
 */



/*****************************************************************************
 * TRANSPORT API
 */

/**
 * @defgroup PJSUA_LIB_TRANSPORT Signaling Transport
 * @ingroup PJSUA_LIB
 * @brief API for managing SIP transports
 * @{
 * SIP transport must be created before adding an account. SIP transport is
 * created by calling #pjsua_transport_create() function.
 */


/** SIP transport identification */
typedef int pjsua_transport_id;


/**
 * STUN configuration.
 */
typedef struct pjsua_stun_config
{
    /**
     * The first STUN server IP address or hostname.
     */
    pj_str_t	stun_srv1;

    /**
     * Port number of the first STUN server.
     * If zero, default STUN port will be used.
     */
    unsigned	stun_port1;
    
    /**
     * Optional second STUN server IP address or hostname, for which the
     * result of the mapping request will be compared to. If the value
     * is empty, only one STUN server will be used.
     */
    pj_str_t	stun_srv2;

    /**
     * Port number of the second STUN server.
     * If zero, default STUN port will be used.
     */
    unsigned	stun_port2;

} pjsua_stun_config;



/**
 * Call this function to initialize STUN config with default values.
 *
 * @param cfg	    The STUN config to be initialized.
 */
PJ_INLINE(void) pjsua_stun_config_default(pjsua_stun_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}


/**
 * Transport configuration for creating UDP transports for both SIP
 * and media.
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
     * Optional address where the socket should be bound.
     */
    pj_in_addr		ip_addr;

    /**
     * Flag to indicate whether STUN should be used.
     */
    pj_bool_t		use_stun;

    /**
     * STUN configuration, must be specified when STUN is used.
     */
    pjsua_stun_config	stun_config;

} pjsua_transport_config;


/**
 * Call this function to initialize UDP config with default values.
 *
 * @param cfg	    The UDP config to be initialized.
 */
PJ_INLINE(void) pjsua_transport_config_default(pjsua_transport_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}


/**
 * Normalize STUN config.
 */
PJ_INLINE(void) pjsua_normalize_stun_config( pjsua_stun_config *cfg )
{
    if (cfg->stun_srv1.slen) {

	if (cfg->stun_port1 == 0)
	    cfg->stun_port1 = 3478;

	if (cfg->stun_srv2.slen == 0) {
	    cfg->stun_srv2 = cfg->stun_srv1;
	    cfg->stun_port2 = cfg->stun_port1;
	} else {
	    if (cfg->stun_port2 == 0)
		cfg->stun_port2 = 3478;
	}

    } else {
	cfg->stun_port1 = 0;
	cfg->stun_srv2.slen = 0;
	cfg->stun_port2 = 0;
    }
}


/**
 * Duplicate transport config.
 */
PJ_INLINE(void) pjsua_transport_config_dup(pj_pool_t *pool,
					   pjsua_transport_config *dst,
					   const pjsua_transport_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));

    if (src->stun_config.stun_srv1.slen) {
	pj_strdup_with_null(pool, &dst->stun_config.stun_srv1,
			    &src->stun_config.stun_srv1);
    }

    if (src->stun_config.stun_srv2.slen) {
	pj_strdup_with_null(pool, &dst->stun_config.stun_srv2,
			    &src->stun_config.stun_srv2);
    }

    pjsua_normalize_stun_config(&dst->stun_config);
}



/**
 * Transport info.
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
 * Create SIP transport.
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
 * Register transport that has been created by application.
 *
 * @param tp		Transport instance.
 * @param p_id		Optional pointer to receive transport ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_register(pjsip_transport *tp,
					      pjsua_transport_id *p_id);


/**
 * Enumerate all transports currently created in the system.
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
 * Close the transport. If transport is forcefully closed, it will be
 * immediately closed, and any pending transactions that are using the
 * transport may not terminate properly. Otherwise, the system will wait
 * until all transactions are closed while preventing new users from
 * using the transport, and will close the transport when it is safe to
 * do so.
 *
 * @param id		Transport ID.
 * @param force		Non-zero to immediately close the transport. This
 *			is not recommended!
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_transport_close( pjsua_transport_id id,
					    pj_bool_t force );

/**
 * @}
 */




/*****************************************************************************
 * ACCOUNT API
 */


/**
 * @defgroup PJSUA_LIB_ACC Account Management
 * @ingroup PJSUA_LIB
 * @brief PJSUA supports multiple accounts..
 * @{
 * PJSUA accounts provide identity (or identities) of the user who is currently
 * using the application. More than one account maybe created with PJSUA.
 *
 * Account may or may not have client registration associated with it.
 * An account is also associated with <b>route set</b> and some <b>authentication
 * credentials</b>, which are used when sending SIP request messages using the
 * account. An account also has presence's <b>online status</b>, which
 * will be reported to remote peer when the subscribe to the account's
 * presence.
 *
 * At least one account MUST be created in the application. If no user
 * association is required, application can create a userless account by
 * calling #pjsua_acc_add_local(). A userless account identifies local endpoint
 * instead of a particular user.
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
#   define PJSUA_REG_INTERVAL	    55
#endif


/**
 * Account configuration.
 */
typedef struct pjsua_acc_config
{
    /** 
     * The full SIP URL for the account. The value can take name address or 
     * URL format, and will look something like "sip:account@serviceprovider".
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
     * Optional URI to be put as Contact for this account. It is recommended
     * that this field is left empty, so that the value will be calculated
     * automatically based on the transport address.
     */
    pj_str_t	    force_contact;

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
     * first).
     */
    pj_str_t	    proxy[PJSUA_ACC_MAX_PROXIES];

    /** 
     * Optional interval for registration, in seconds. If the value is zero, 
     * default interval will be used (PJSUA_REG_INTERVAL, 55 seconds).
     */
    unsigned	    reg_timeout;

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

} pjsua_acc_config;


/**
 * Call this function to initialize account config with default values.
 *
 * @param cfg	    The account config to be initialized.
 */
PJ_INLINE(void) pjsua_acc_config_default(pjsua_acc_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->reg_timeout = PJSUA_REG_INTERVAL;
}



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
     * An up to date expiration interval for account registration session.
     */
    int			expires;

    /**
     * Last registration status code. If status code is zero, the account
     * is currently not registered. Any other value indicates the SIP
     * status code of the registration.
     */
    pjsip_status_code	status;

    /**
     * String describing the registration status.
     */
    pj_str_t		status_text;

    /**
     * Presence online status for this account.
     */
    pj_bool_t		online_status;

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
 * Add a new account to pjsua. PJSUA must have been initialized (with
 * #pjsua_init()) before calling this function.
 *
 * @param cfg		Account configuration.
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
PJ_DECL(pj_status_t) pjsua_acc_add(const pjsua_acc_config *cfg,
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
 * Delete account.
 *
 * @param acc_id	Id of the account to be deleted.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_del(pjsua_acc_id acc_id);


/**
 * Modify account information.
 *
 * @param acc_id	Id of the account to be modified.
 * @param cfg		New account configuration.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_modify(pjsua_acc_id acc_id,
				      const pjsua_acc_config *cfg);


/**
 * Modify account's presence status to be advertised to remote/presence
 * subscribers.
 *
 * @param acc_id	The account ID.
 * @param is_online	True of false.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_online_status(pjsua_acc_id acc_id,
						 pj_bool_t is_online);


/**
 * Update registration or perform unregistration. 
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
 * Get account information.
 *
 * @param acc_id	Account identification.
 * @param info		Pointer to receive account information.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_acc_get_info(pjsua_acc_id acc_id,
					pjsua_acc_info *info);


/**
 * Enum accounts all account ids.
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
 * Enum accounts info.
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
 * Create a suitable URI to be put as Contact based on the specified
 * target URI for the specified account.
 *
 * @param pool		Pool to allocate memory for the string.
 * @param contact	The string where the Contact URI will be stored.
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
 * Create a suitable URI to be put as Contact based on the information
 * in the incoming request.
 *
 * @param pool		Pool to allocate memory for the string.
 * @param contact	The string where the Contact URI will be stored.
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
 * @}
 */


/*****************************************************************************
 * CALLS API
 */


/**
 * @defgroup PJSUA_LIB_CALL Calls
 * @ingroup PJSUA_LIB
 * @brief Call manipulation.
 * @{
 */

/**
 * Max simultaneous calls.
 */
#ifndef PJSUA_MAX_CALLS
#   define PJSUA_MAX_CALLS	    32
#endif



/**
 * Call media status.
 */
typedef enum pjsua_call_media_status
{
    PJSUA_CALL_MEDIA_NONE,
    PJSUA_CALL_MEDIA_ACTIVE,
    PJSUA_CALL_MEDIA_LOCAL_HOLD,
    PJSUA_CALL_MEDIA_REMOTE_HOLD,
} pjsua_call_media_status;


/**
 * Call info.
 */
typedef struct pjsua_call_info
{
    /** Call identification. */
    pjsua_call_id	id;

    /** Initial call role (UAC == caller) */
    pjsip_role_e	role;

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

    /** Call state */
    pjsip_inv_state	state;

    /** Text describing the state */
    pj_str_t		state_text;

    /** Last status code heard, which can be used as cause code */
    pjsip_status_code	last_status;

    /** The reason phrase describing the status. */
    pj_str_t		last_status_text;

    /** Call media status. */
    pjsua_call_media_status media_status;

    /** Media direction */
    pjmedia_dir		media_dir;

    /** The conference port number for the call */
    pjsua_conf_port_id	conf_slot;

    /** Up-to-date call connected duration (zero when call is not 
     *  established)
     */
    pj_time_val		connect_duration;

    /** Total call duration, including set-up time */
    pj_time_val		total_duration;

    /** Internal */
    struct {
	char	local_info[128];
	char	local_contact[128];
	char	remote_info[128];
	char	remote_contact[128];
	char	call_id[128];
	char	last_status_text[128];
    } buf_;

} pjsua_call_info;



/**
 * Get maximum number of calls configured in pjsua.
 *
 * @return		Maximum number of calls configured.
 */
PJ_DECL(unsigned) pjsua_call_get_max_count(void);

/**
 * Get number of currently active calls.
 *
 * @return		Number of currently active calls.
 */
PJ_DECL(unsigned) pjsua_call_get_count(void);

/**
 * Enumerate all active calls.
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
 * @param options	Options (must be zero at the moment).
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
					  unsigned options,
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
 * Attach application specific data to the call.
 *
 * @param call_id	Call identification.
 * @param user_data	Arbitrary data to be attached to the call.
 *
 * @return		The user data.
 */
PJ_DECL(pj_status_t) pjsua_call_set_user_data(pjsua_call_id call_id,
					      void *user_data);


/**
 * Get user data attached to the call.
 *
 * @param call_id	Call identification.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pjsua_call_get_user_data(pjsua_call_id call_id);


/**
 * Send response to incoming INVITE request.
 *
 * @param call_id	Incoming call identification.
 * @param code		Status code, (100-699).
 * @param reason	Optional reason phrase. If NULL, default text
 *			will be used.
 * @param msg_data	Optional list of headers etc to be added to outgoing
 *			response message.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_answer(pjsua_call_id call_id, 
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data);

/**
 * Hangup call by using method that is appropriate according to the
 * call state.
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
 * Put the specified call on hold.
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
 * Send re-INVITE (to release hold).
 *
 * @param call_id	Call identification.
 * @param unhold	If this argument is non-zero and the call is locally
 *			held, this will release the local hold.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_reinvite(pjsua_call_id call_id,
					 pj_bool_t unhold,
					 const pjsua_msg_data *msg_data);


/**
 * Initiate call transfer to the specified address.
 *
 * @param call_id	Call identification.
 * @param dest		Address of new target to be contacted.
 * @param msg_data	Optional message components to be sent with
 *			the request.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_xfer(pjsua_call_id call_id, 
				     const pj_str_t *dest,
				     const pjsua_msg_data *msg_data);

/**
 * Send DTMF digits to remote using RFC 2833 payload formats.
 *
 * @param call_id	Call identification.
 * @param digits	DTMF digits to be sent.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_call_dial_dtmf(pjsua_call_id call_id, 
					  const pj_str_t *digits);

/**
 * Send instant messaging inside INVITE session.
 *
 * @param call_id	Call identification.
 * @param mime_type	Optional MIME type. If NULL, then "text/plain" is 
 *			assumed.
 * @param content	The message content.
 * @param msg_data	Optional list of headers etc to be included in outgoing
 *			request. The body descriptor in the msg_data is 
 *			ignored.
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
 * Terminate all calls.
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
 * @}
 */


/*****************************************************************************
 * BUDDY API
 */


/**
 * @defgroup PJSUA_LIB_BUDDY Buddy, Presence, and Instant Messaging
 * @ingroup PJSUA_LIB
 * @brief Buddy management, buddy's presence, and instant messaging.
 * @{
 */

/**
 * Max buddies in buddy list.
 */
#ifndef PJSUA_MAX_BUDDIES
#   define PJSUA_MAX_BUDDIES	    256
#endif


/**
 * Buddy configuration.
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

} pjsua_buddy_config;


/**
 * Buddy's online status.
 */
typedef enum pjsua_buddy_status
{
    /**
     * Online status is unknown (possibly because no presence subscription
     * has been established).
     */
    PJSUA_BUDDY_STATUS_UNKNOWN,

    /**
     * Buddy is known to be offline.
     */
    PJSUA_BUDDY_STATUS_ONLINE,

    /**
     * Buddy is offline.
     */
    PJSUA_BUDDY_STATUS_OFFLINE,

} pjsua_buddy_status;



/**
 * Buddy info.
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
     * Internal buffer.
     */
    char		buf_[256];

} pjsua_buddy_info;


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
 * Enum buddy IDs.
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
 * Add new buddy.
 *
 * @param cfg		Buddy configuration.
 * @param p_buddy_id	Pointer to receive buddy ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_add(const pjsua_buddy_config *cfg,
				     pjsua_buddy_id *p_buddy_id);


/**
 * Delete buddy.
 *
 * @param buddy_id	Buddy identification.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_buddy_del(pjsua_buddy_id buddy_id);


/**
 * Enable/disable buddy's presence monitoring.
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
 * Dump presence subscriptions to log file.
 *
 * @param verbose	Yes or no.
 */
PJ_DECL(void) pjsua_pres_dump(pj_bool_t verbose);


/**
 * The MESSAGE method (defined in pjsua_im.c)
 */
extern const pjsip_method pjsip_message_method;



/**
 * Send instant messaging outside dialog, using the specified account for
 * route set and authentication.
 *
 * @param acc_id	Account ID to be used to send the request.
 * @param to		Remote URI.
 * @param mime_type	Optional MIME type. If NULL, then "text/plain" is 
 *			assumed.
 * @param content	The message content.
 * @param msg_data	Optional list of headers etc to be included in outgoing
 *			request. The body descriptor in the msg_data is 
 *			ignored.
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
 * @defgroup PJSUA_LIB_MEDIA Media
 * @ingroup PJSUA_LIB
 * @brief Media manipulation.
 * @{
 *
 * PJSUA has rather powerful media features, which are built around the
 * PJMEDIA conference bridge. Basically, all media termination (such as
 * calls, file players, file recorders, sound device, tone generators, etc)
 * are terminated in the conference bridge, and application can manipulate
 * the interconnection between these terminations freely. If more than
 * one media terminations are terminated in the same slot, the conference
 * bridge will mix the signal automatically.
 *
 * Application connects one media termination/slot to another by calling
 * #pjsua_conf_connect() function. This will establish <b>unidirectional</b>
 * media flow from the source termination to the sink termination. For
 * example, to stream a WAV file to remote call, application may use
 * the following steps:
 *
 \code
  
  pj_status_t stream_to_call( pjsua_call_id call_id )
  {
     pjsua_player_id player_id;
     
     status = pjsua_player_create("mysong.wav", 0, NULL, &player_id);
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
 * Max ports in the conference bridge.
 */
#ifndef PJSUA_MAX_CONF_PORTS
#   define PJSUA_MAX_CONF_PORTS	    254
#endif



/**
 * Media configuration.
 */
struct pjsua_media_config
{
    /**
     * Clock rate to be applied to the conference bridge.
     * If value is zero, default clock rate will be used (16KHz).
     */
    unsigned		clock_rate;

    /**
     * Specify maximum number of media ports to be created in the
     * conference bridge. Since all media terminate in the bridge
     * (calls, file player, file recorder, etc), the value must be
     * large enough to support all of them. However, the larger
     * the value, the more computations are performed.
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
     *   8-10: resampling use large filter,
     *   3-7:  resampling use small filter,
     *   1-2:  resampling use linear.
     * The media quality also sets speex codec quality/complexity to the
     * number.
     *
     * Default: 6.
     */
    unsigned		quality;

    /**
     * Specify default ptime.
     *
     * Default: 0 (codec specific)
     */
    unsigned		ptime;

    /**
     * Disable VAD?
     *
     * Default: 0 (no (meaning VAD is enabled))
     */
    pj_bool_t		no_vad;

    /**
     * iLBC mode (20 or 30).
     *
     * Default: 20
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
     * Echo canceller tail length, in miliseconds.
     *
     * Default: 256
     */
    unsigned		ec_tail_len;
};


/**
 * Use this function to initialize media config.
 *
 * @param cfg	The media config to be initialized.
 */
PJ_INLINE(void) pjsua_media_config_default(pjsua_media_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->clock_rate = 16000;
    cfg->max_media_ports = 32;
    cfg->has_ioqueue = PJ_TRUE;
    cfg->thread_cnt = 1;
    cfg->quality = 6;
    cfg->ilbc_mode = 20;
    cfg->ec_tail_len = 256;
}



/**
 * Codec config.
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
     * Internal buffer.
     */
    char		buf_[32];

} pjsua_codec_info;


/**
 * Conference port info.
 */
typedef struct pjsua_conf_port_info
{
    /** Conference port number. */
    pjsua_conf_port_id	slot_id;

    /** Port name. */
    pj_str_t		name;

    /** Clock rate. */
    unsigned		clock_rate;

    /** Number of channels. */
    unsigned		channel_count;

    /** Samples per frame */
    unsigned		samples_per_frame;

    /** Bits per sample */
    unsigned		bits_per_sample;

    /** Number of listeners in the array. */
    unsigned		listener_cnt;

    /** Array of listeners (in other words, ports where this port is 
     *  transmitting to.
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
 * @param id		Port identification.
 * @param info		Pointer to store the port info.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_get_port_info( pjsua_conf_port_id id,
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
 * call this function if it registered the port manually.
 *
 * @param id		The slot id of the port to be removed.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_remove_port(pjsua_conf_port_id id);


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
 * Disconnect media flow from the source to destination port.
 *
 * @param source	Port ID of the source media/transmitter.
 * @param sink		Port ID of the destination media/received.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_conf_disconnect(pjsua_conf_port_id source,
					   pjsua_conf_port_id sink);


/*****************************************************************************
 * File player.
 */

/**
 * Create a file player, and automatically connect this player to
 * the conference bridge.
 *
 * @param filename	The filename to be played. Currently only
 *			WAV files are supported, and the WAV file MUST be
 *			formatted as 16bit PCM mono/single channel (any
 *			clock rate is supported).
 * @param options	Options (currently zero).
 * @param p_id		Pointer to receive player ID.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_player_create(const pj_str_t *filename,
					 unsigned options,
					 pjsua_player_id *p_id);


/**
 * Get conference port ID associated with player.
 *
 * @param id		The file player ID.
 *
 * @return		Conference port ID associated with this player.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_player_id id);


/**
 * Set playback position.
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
 * Close the file, remove the player from the bridge, and free
 * resources associated with the file player.
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
 * the conference bridge.
 *
 * @param filename	Output file name.
 * @param file_format	Specify the file format (currently only WAV is
 *			supported, so the value MUST be zero).
 * @param encoding	Specify the encoding to be applied to the file.
 *			Currently only 16bit raw PCM is supported, so
 *			the value must be NULL.
 * @param max_size	Maximum file size. Specify -1 to remove size
 *			limitation.
 * @param options	Optional options.
 * @param p_id		Pointer to receive the recorder instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_recorder_create(const pj_str_t *filename,
					   unsigned file_format,
					   const pj_str_t *encoding,
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
 * Enum sound devices.
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
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 *
 * @param capture_dev   Device ID of the capture device.
 * @param playback_dev	Device ID of the playback device.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_set_snd_dev(int capture_dev,
				       int playback_dev);


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
 * @param id		Codec ID.
 * @param priority	Codec priority, 0-255, where zero means to disable
 *			the codec.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_set_priority( const pj_str_t *id,
					       pj_uint8_t priority );


/**
 * Get codec parameters.
 *
 * @param id		Codec ID.
 * @param param		Structure to receive codec parameters.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_get_param( const pj_str_t *id,
					    pjmedia_codec_param *param );


/**
 * Set codec parameters.
 *
 * @param id		Codec ID.
 * @param param		Codec parameter to set.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_codec_set_param( const pj_str_t *id,
					    const pjmedia_codec_param *param);




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


/**
 * @}
 */



PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJSUA_H__ */

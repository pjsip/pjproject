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


PJ_BEGIN_DECL


/**
 * Max buddies in buddy list.
 */
#ifndef PJSUA_MAX_BUDDIES
#   define PJSUA_MAX_BUDDIES	    256
#endif


/**
 * Max simultaneous calls.
 */
#ifndef PJSUA_MAX_CALLS
#   define PJSUA_MAX_CALLS	    256
#endif


/**
 * Maximum accounts.
 */
#ifndef PJSUA_MAX_ACC
#   define PJSUA_MAX_ACC	    32
#endif


/**
 * Account configuration.
 */
struct pjsua_acc_config
{
    /** SIP URL for account ID (mandatory) */
    pj_str_t	    id;

    /** Registrar URI (mandatory) */
    pj_str_t	    reg_uri;

    /** Optional contact URI */
    pj_str_t	    contact;

    /** Service proxy (default: none) */
    pj_str_t	    proxy;

    /** Default timeout (mandatory) */
    pj_int32_t	    reg_timeout;

    /** Number of credentials. */
    unsigned	    cred_count;

    /** Array of credentials. */
    pjsip_cred_info cred_info[4];

};


/**
 * @see pjsua_acc_config
 */
typedef struct pjsua_acc_config pjsua_acc_config;


/**
 * PJSUA settings.
 */
struct pjsua_config
{
    /** SIP UDP signaling port. Set to zero to disable UDP signaling,
     * which in this case application must manually add a transport
     * to SIP endpoint.
     * (default: 5060) 
     */
    unsigned	udp_port;

    /** Optional hostname or IP address to publish as the host part of
     *  Contact header. This must be specified if UDP transport is
     *  disabled.
     *  (default: NULL)
     */
    pj_str_t	sip_host;

    /** Optional port number to publish in the port part of Contact header.
     *  This must be specified if UDP transport is disabled.
     *  (default: 0)
     */
    unsigned	sip_port;

    /** Start of RTP port. Set to zero to prevent pjsua from creating
     *  media transports, which in this case application must manually
     *  create media transport for each calls.
     *  (default: 4000) 
     */
    unsigned	start_rtp_port;

    /** Maximum calls to support (default: 4) */
    unsigned	max_calls;

    /** Maximum slots in the conference bridge (default: 0/calculated
     *  as max_calls*2
     */
    unsigned	conf_ports;

    /** Number of worker threads (value >=0, default: 1) */
    unsigned	thread_cnt;

    /** Separate ioqueue for media? (default: yes) */
    pj_bool_t	media_has_ioqueue;

    /** Number of worker thread for media (value >=0, default: 1) */
    unsigned	media_thread_cnt;

    /** First STUN server IP address. When STUN is configured, then the
     *  two STUN server settings must be fully set.
     *  (default: none) 
     */
    pj_str_t	stun_srv1;

    /** First STUN port number */
    unsigned	stun_port1;

    /** Second STUN server IP address */
    pj_str_t	stun_srv2;

    /** Second STUN server port number */
    unsigned	stun_port2;

    /** Sound player device ID (default: 0) */
    unsigned	snd_player_id;

    /** Sound capture device ID (default: 0) */
    unsigned	snd_capture_id;

    /** Internal clock rate (to be applied to sound devices and conference
     *  bridge, default is 0/follows the codec, or 44100 for MacOS).
     */
    unsigned	clock_rate;

    /** Do not use sound device (default: 0). */
    pj_bool_t	null_audio;

    /** WAV file to load for auto_play (default: NULL) */
    pj_str_t	wav_file;

    /** Auto play WAV file for calls? (default: no) */
    pj_bool_t	auto_play;

    /** Auto loopback calls? (default: no) */
    pj_bool_t	auto_loop;

    /** Automatically put calls to conference? (default: no) */
    pj_bool_t	auto_conf;

    /** Speex codec complexity? (default: 10) */
    unsigned	complexity;

    /** Speex codec quality? (default: 10) */
    unsigned	quality;

    /** Codec ptime? (default: 0 (follows the codec)) */
    unsigned	ptime;

    /** Number of additional codecs/"--add-codec" with pjsua (default: 0) */
    unsigned	codec_cnt;

    /** Additional codecs/"--add-codec" options */
    pj_str_t	codec_arg[32];

    /** SIP status code to be automatically sent to incoming calls
     *  (default: 100).
     */
    unsigned	auto_answer;

    /** Periodic time to refresh call with re-INVITE (default: 0)
     */
    unsigned	uas_refresh;

    /** Maximum incoming call duration (default: 3600) */
    unsigned	uas_duration;

    /** Outbound proxy (default: none) */
    pj_str_t	outbound_proxy;

    /** URI to call.			*/
    pj_str_t	uri_to_call;

    /** Number of SIP accounts */
    unsigned	acc_cnt;

    /** SIP accounts configuration */
    pjsua_acc_config	acc_config[32];

    /** Logging verbosity (default: 5).	*/
    unsigned	log_level;

    /** Logging to be displayed to stdout (default: 4) */
    unsigned	app_log_level;

    /** Log decoration */
    unsigned	log_decor;

    /** Optional log filename (default: NULL) */
    pj_str_t	log_filename;

    /** Number of buddies in address book (default: 0) */
    unsigned	buddy_cnt;

    /** Buddies URI */
    pj_str_t	buddy_uri[256];
};


/**
 * @see pjsua_config
 */
typedef struct pjsua_config pjsua_config;



/**
 * Application callbacks.
 */
struct pjsua_callback
{
    /**
     * Notify application when invite state has changed.
     * Application may then query the call info to get the
     * detail call states.
     */
    void (*on_call_state)(int call_index, pjsip_event *e);

    /**
     * Notify application on call being transfered.
     * Application can decide to accept/reject transfer request
     * by setting the code (default is 200). When this callback
     * is not defined, the default behavior is to accept the
     * transfer.
     */
    void (*on_call_transfered)(int call_index,
			       const pj_str_t *dst,
			       pjsip_status_code *code);

    /**
     * Notify application when registration status has changed.
     * Application may then query the account info to get the
     * registration details.
     */
    void (*on_reg_state)(int acc_index);

    /**
     * Notify application when the buddy state has changed.
     * Application may then query the buddy into to get the details.
     */
    void (*on_buddy_state)(int buddy_index);

    /**
     * Notify application on incoming pager (i.e. MESSAGE request).
     * Argument call_index will be -1 if MESSAGE request is not related to an 
     * existing call.
     */
    void (*on_pager)(int call_index, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *txt);

    /**
     * Notify application about typing indication.
     */
    void (*on_typing)(int call_index, const pj_str_t *from,
		      const pj_str_t *to, pj_bool_t is_typing);

};

/**
 * @see pjsua_callback
 */
typedef struct pjsua_callback pjsua_callback;


/**
 * Call info.
 */
struct pjsua_call_info
{
    unsigned		index;
    pj_bool_t		active;
    pjsip_role_e	role;
    pj_str_t		local_info;
    pj_str_t		remote_info;
    pjsip_inv_state	state;
    pj_str_t		state_text;
    pj_time_val		connect_duration;
    pj_time_val		total_duration;
    pjsip_status_code	cause;
    pj_str_t		cause_text;
    pj_bool_t		has_media;
    unsigned		conf_slot;
};

typedef struct pjsua_call_info pjsua_call_info;


enum pjsua_buddy_status
{
    PJSUA_BUDDY_STATUS_UNKNOWN,
    PJSUA_BUDDY_STATUS_ONLINE,
    PJSUA_BUDDY_STATUS_OFFLINE,
};

typedef enum pjsua_buddy_status pjsua_buddy_status;


/**
 * Buddy info.
 */
struct pjsua_buddy_info
{
    unsigned		index;
    pj_bool_t		is_valid;
    pj_str_t		name;
    pj_str_t		display_name;
    pj_str_t		host;
    unsigned		port;
    pj_str_t		uri;
    pjsua_buddy_status	status;
    pj_str_t		status_text;
    pj_bool_t		monitor;
    int			acc_index;
};

typedef struct pjsua_buddy_info pjsua_buddy_info;


/**
 * Account info.
 */
struct pjsua_acc_info
{
    unsigned		index;
    pj_str_t		acc_id;
    pj_bool_t		has_registration;
    int			expires;
    pjsip_status_code	status;
    pj_str_t		status_text;
    pj_bool_t		online_status;
    char		buf[PJ_ERR_MSG_SIZE];
};

typedef struct pjsua_acc_info pjsua_acc_info;


typedef int pjsua_player_id;
typedef int pjsua_recorder_id;



/*****************************************************************************
 * PJSUA API (defined in pjsua_core.c).
 */

/**
 * Initialize pjsua settings with default parameters.
 */
PJ_DECL(void) pjsua_default_config(pjsua_config *cfg);


/**
 * Validate configuration.
 */
PJ_DECL(pj_status_t) pjsua_test_config(const pjsua_config *cfg,
				       char *errmsg,
				       int len);


/**
 * Instantiate pjsua application. This initializes pjlib/pjlib-util, and 
 * creates memory pool factory to be used by application.
 */
PJ_DECL(pj_status_t) pjsua_create(void);


/**
 * Initialize pjsua application with the specified settings.
 *
 * This will initialize all libraries, create endpoint instance, and register
 * pjsip modules. 
 *
 * Application may register module after calling this function.
 */
PJ_DECL(pj_status_t) pjsua_init(const pjsua_config *cfg,
				const pjsua_callback *cb);


/**
 * Start pjsua stack. Application calls this after pjsua settings has been
 * configured.
 *
 * This will start the transport, worker threads (if any), and registration 
 * process, if registration is configured.
 */
PJ_DECL(pj_status_t) pjsua_start(void);

/**
 * Destroy pjsua.
 */
PJ_DECL(pj_status_t) pjsua_destroy(void);

/**
 * Poll pjsua.
 */
PJ_DECL(int) pjsua_handle_events(unsigned msec_timeout);


/**
 * Get SIP endpoint instance.
 * Only valid after pjsua_init().
 */
PJ_DECL(pjsip_endpoint*) pjsua_get_pjsip_endpt(void);

/**
 * Get media endpoint instance.
 * Only valid after pjsua_init().
 */
PJ_DECL(pjmedia_endpt*) pjsua_get_pjmedia_endpt(void);

/**
 * Replace media transport.
 */
PJ_DECL(pj_status_t) pjsua_set_call_media_transport(unsigned call_index,
						    const pjmedia_sock_info *i,
						    pjmedia_transport *tp);


/*****************************************************************************
 * PJSUA Call API (defined in pjsua_call.c).
 */

/**
 * Get maximum number of calls configured in pjsua.
 */
PJ_DECL(unsigned) pjsua_get_max_calls(void);

/**
 * Get current number of active calls.
 */
PJ_DECL(unsigned) pjsua_get_call_count(void);

/**
 * Check if the specified call has active INVITE session and the INVITE
 * session has not been disconnected.
 */
PJ_DECL(pj_bool_t) pjsua_call_is_active(unsigned call_index);


/**
 * Check if call has a media session.
 */
PJ_DECL(pj_bool_t) pjsua_call_has_media(unsigned call_index);


/**
 * Get call info.
 */
PJ_DECL(pj_status_t) pjsua_get_call_info(unsigned call_index,
					 pjsua_call_info *info);


/**
 * Duplicate call info.
 */
PJ_DECL(void) pjsua_dup_call_info(pj_pool_t *pool,
				  pjsua_call_info *dst_info,
				  const pjsua_call_info *src_info);


/**
 * Make outgoing call.
 */
PJ_DECL(pj_status_t) pjsua_make_call(unsigned acc_index,
				     const pj_str_t *dst_uri,
				     int *p_call_index);


/**
 * Answer call.
 */
PJ_DECL(void) pjsua_call_answer(int call_index, int code);

/**
 * Hangup call.
 */
PJ_DECL(void) pjsua_call_hangup(int call_index);


/**
 * Put call on-hold.
 */
PJ_DECL(void) pjsua_call_set_hold(int call_index);


/**
 * Send re-INVITE (to release hold).
 */
PJ_DECL(void) pjsua_call_reinvite(int call_index);


/**
 * Transfer call.
 */
PJ_DECL(void) pjsua_call_xfer(unsigned call_index, const pj_str_t *dest);

/**
 * Dial DTMF.
 */
PJ_DECL(pj_status_t) pjsua_call_dial_dtmf(unsigned call_index, 
					  const pj_str_t *digits);


/**
 * Send instant messaging inside INVITE session.
 */
PJ_DECL(void) pjsua_call_send_im(int call_index, const pj_str_t *text);


/**
 * Send IM typing indication inside INVITE session.
 */
PJ_DECL(void) pjsua_call_typing(int call_index, pj_bool_t is_typing);

/**
 * Terminate all calls.
 */
PJ_DECL(void) pjsua_call_hangup_all(void);


/**
 * Dump call and media statistics to string.
 */
PJ_DECL(void) pjsua_dump_call(int call_index, int with_media, 
			      char *buffer, unsigned maxlen,
			      const char *indent);


/*****************************************************************************
 * PJSUA Account and Client Registration API (defined in pjsua_reg.c).
 */


/**
 * Get number of accounts.
 */
PJ_DECL(unsigned) pjsua_get_acc_count(void);

/**
 * Get account info.
 */
PJ_DECL(pj_status_t) pjsua_acc_get_info(unsigned acc_index,
					pjsua_acc_info *info);

/**
 * Add a new account.
 * This function should be called after pjsua_init().
 * Application should call pjsua_acc_set_registration() to start 
 * registration for this account.
 */
PJ_DECL(pj_status_t) pjsua_acc_add(const pjsua_acc_config *cfg,
				   int *acc_index);


/**
 * Set account's presence status.
 * Must call pjsua_pres_refresh() after this.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_online_status(unsigned acc_index,
						 pj_bool_t is_online);


/**
 * Update registration or perform unregistration. If renew argument is zero,
 * this will start unregistration process.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_registration(unsigned acc_index, pj_bool_t renew);




/*****************************************************************************
 * PJSUA Presence (pjsua_pres.c)
 */

/**
 * Get buddy count.
 */
PJ_DECL(unsigned) pjsua_get_buddy_count(void);


/**
 * Get buddy info.
 */
PJ_DECL(pj_status_t) pjsua_buddy_get_info(unsigned buddy_index,
					  pjsua_buddy_info *info);

/**
 * Add new buddy.
 */
PJ_DECL(pj_status_t) pjsua_buddy_add(const pj_str_t *uri,
				     int *buddy_index);


/**
 * Enable/disable buddy's presence monitoring.
 * Must call pjsua_pres_refresh() after this.
 */
PJ_DECL(pj_status_t) pjsua_buddy_subscribe_pres(unsigned buddy_index,
						pj_bool_t monitor);


/**
 * Refresh both presence client and server subscriptions.
 */
PJ_DECL(void) pjsua_pres_refresh(void);

/**
 * Dump presence subscriptions.
 */
PJ_DECL(void) pjsua_pres_dump(pj_bool_t detail);


/*****************************************************************************
 * PJSUA Instant Messaging (pjsua_im.c)
 */

/**
 * The MESSAGE method (defined in pjsua_im.c)
 */
extern const pjsip_method pjsip_message_method;



/**
 * Send IM outside dialog.
 */
PJ_DECL(pj_status_t) pjsua_im_send(int acc_index, const pj_str_t *dst_uri, 
				   const pj_str_t *text);


/**
 * Send typing indication outside dialog.
 */
PJ_DECL(pj_status_t) pjsua_im_typing(int acc_index, const pj_str_t *dst_uri, 
				     pj_bool_t is_typing);



/*****************************************************************************
 * Media.
 */

/**
 * Get maxinum number of conference ports.
 */
PJ_DECL(unsigned) pjsua_conf_max_ports(void);


/**
 * Enum all conference ports.
 */
PJ_DECL(pj_status_t) pjsua_conf_enum_ports(unsigned *count,
					   pjmedia_conf_port_info info[]);


/**
 * Connect conference port.
 */
PJ_DECL(pj_status_t) pjsua_conf_connect(unsigned src_port,
					unsigned dst_port);


/**
 * Connect conference port connection.
 */
PJ_DECL(pj_status_t) pjsua_conf_disconnect(unsigned src_port,
					   unsigned dst_port);


/**
 * Create a file player.
 */
PJ_DECL(pj_status_t) pjsua_player_create(const pj_str_t *filename,
					 pjsua_player_id *id);


/**
 * Get conference port associated with player.
 */
PJ_DECL(int) pjsua_player_get_conf_port(pjsua_player_id id);


/**
 * Set playback position.
 */
PJ_DECL(pj_status_t) pjsua_player_set_pos(pjsua_player_id id,
					  pj_uint32_t samples);


/**
 * Destroy player.
 */
PJ_DECL(pj_status_t) pjsua_player_destroy(pjsua_player_id id);



/**
 * Create a file recorder.
 */
PJ_DECL(pj_status_t) pjsua_recorder_create(const pj_str_t *filename,
					   pjsua_recorder_id *id);


/**
 * Get conference port associated with recorder.
 */
PJ_DECL(int) pjsua_recorder_get_conf_port(pjsua_recorder_id id);


/**
 * Destroy recorder (will complete recording).
 */
PJ_DECL(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id);


/**
 * Enum sound devices.
 */
PJ_DECL(pj_status_t) pjsua_enum_snd_devices(unsigned *count,
					    pjmedia_snd_dev_info info[]);


/**
 * Select or change sound device.
 * This will only change the device ID in configuration (not changing
 * the current device).
 */
PJ_DECL(pj_status_t) pjsua_set_snd_dev(int snd_capture_id,
				       int snd_player_id);


/*****************************************************************************
 * Utilities.
 *
 */

/** String to describe invite session states */
extern const char *pjsua_inv_state_names[];

/**
 * Parse arguments (pjsua_opt.c).
 */
PJ_DECL(pj_status_t) pjsua_parse_args(int argc, char *argv[],
				      pjsua_config *cfg);

/**
 * Load settings from a file.
 */
PJ_DECL(pj_status_t) pjsua_load_settings(const char *filename,
					 pjsua_config *cfg);

/**
 * Get pjsua running config.
 */
PJ_DECL(const pjsua_config*) pjsua_get_config(void);


/**
 * Dump settings.
 * If cfg is NULL, it will dump current settings.
 */
PJ_DECL(int) pjsua_dump_settings(const pjsua_config *cfg,
				 char *buf, pj_size_t max);

/**
 * Save settings to a file.
 */
PJ_DECL(pj_status_t) pjsua_save_settings(const char *filename,
					 const pjsua_config *cfg);


/*
 * Verify that valid SIP url is given.
 * @return  PJ_SUCCESS if valid.
 */
PJ_DECL(pj_status_t) pjsua_verify_sip_url(const char *c_url);

/*
 * Dump application states.
 */
PJ_DECL(void) pjsua_dump(pj_bool_t detail);

/**
 * Display error message for the specified error code.
 */
PJ_DECL(void) pjsua_perror(const char *sender, const char *title, 
			   pj_status_t status);




PJ_END_DECL


#endif	/* __PJSUA_H__ */

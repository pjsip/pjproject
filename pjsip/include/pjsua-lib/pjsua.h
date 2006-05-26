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
 * Structure to be attached to invite dialog. 
 * Given a dialog "dlg", application can retrieve this structure
 * by accessing dlg->mod_data[pjsua.mod.id].
 */
struct pjsua_call
{
    unsigned		 index;	    /**< Index in pjsua array.		    */
    pjsip_inv_session	*inv;	    /**< The invite session.		    */
    pj_time_val		 start_time;/**< First INVITE sent/received.	    */
    pj_time_val		 res_time;  /**< First response sent/received.	    */
    pj_time_val		 conn_time; /**< Connected/confirmed time.	    */
    pj_time_val		 dis_time;  /**< Disconnect time.		    */
    int			 acc_index; /**< Account index being used.	    */
    pjmedia_session	*session;   /**< The media session.		    */
    unsigned		 conf_slot; /**< Slot # in conference bridge.	    */
    pjsip_evsub		*xfer_sub;  /**< Xfer server subscription, if this
					 call was triggered by xfer.	    */
    pjmedia_sock_info	 skinfo;    /**< Preallocated media sockets.	    */
    pjmedia_transport	*med_tp;    /**< Media transport.		    */
    void		*app_data;  /**< Application data.		    */
    pj_timer_entry	 refresh_tm;/**< Timer to send re-INVITE.	    */
    pj_timer_entry	 hangup_tm; /**< Timer to hangup call.		    */
};

typedef struct pjsua_call pjsua_call;


/**
 * Buddy data.
 */
struct pjsua_buddy
{
    pj_str_t		 uri;	    /**< Buddy URI		        */
    int			 acc_index; /**< Which account to use.		*/
    pj_bool_t		 monitor;   /**< Should we monitor?		*/
    pjsip_evsub		*sub;	    /**< Buddy presence subscription	*/
    pjsip_pres_status	 status;    /**< Buddy presence status.		*/
};

typedef struct pjsua_buddy pjsua_buddy;


/**
 * Server presence subscription list head.
 */
struct pjsua_srv_pres
{
    PJ_DECL_LIST_MEMBER(struct pjsua_srv_pres);
    pjsip_evsub	    *sub;
    char	    *remote;
};

typedef struct pjsua_srv_pres pjsua_srv_pres;


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
 * Account
 */
struct pjsua_acc
{
    int		     index;	    /**< Index in accounts array.	*/
    pj_str_t	     user_part;	    /**< User part of local URI.	*/
    pj_str_t	     host_part;	    /**< Host part of local URI.	*/

    pjsip_regc	    *regc;	    /**< Client registration session.   */
    pj_timer_entry   reg_timer;	    /**< Registration timer.		*/
    pj_status_t	     reg_last_err;  /**< Last registration error.	*/
    int		     reg_last_code; /**< Last status last register.	*/

    pjsip_route_hdr  route_set;	    /**< Route set.			*/

    pj_bool_t	     online_status; /**< Our online status.		*/
    pjsua_srv_pres   pres_srv_list; /**< Server subscription list.	*/

    void	    *app_data;	    /**< Application data.		*/
};


/**
 * @see pjsua_acc
 */
typedef struct pjsua_acc pjsua_acc;


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

    /** Number of worker threads (default: 1) */
    unsigned	thread_cnt;

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
     * Notify UI when invite state has changed.
     */
    void (*on_call_state)(int call_index, pjsip_event *e);

    /**
     * Notify UI when registration status has changed.
     */
    void (*on_reg_state)(int acc_index);

    /**
     * Notify UI on incoming pager (i.e. MESSAGE request).
     * Argument call_index will be -1 if MESSAGE request is not related to an 
     * existing call.
     */
    void (*on_pager)(int call_index, const pj_str_t *from,
		     const pj_str_t *to, const pj_str_t *txt);

    /**
     * Notify UI about typing indication.
     */
    void (*on_typing)(int call_index, const pj_str_t *from,
		      const pj_str_t *to, pj_bool_t is_typing);

};

/**
 * @see pjsua_callback
 */
typedef struct pjsua_callback pjsua_callback;


/* PJSUA application variables. */
struct pjsua
{
    /* Control: */
    pj_caching_pool  cp;	    /**< Global pool factory.		*/
    pjsip_endpoint  *endpt;	    /**< Global endpoint.		*/
    pj_pool_t	    *pool;	    /**< pjsua's private pool.		*/
    pjsip_module     mod;	    /**< pjsua's PJSIP module.		*/

    
    /* Config: */
    pjsua_config    config;	    /**< PJSUA configs			*/

    /* Application callback: */
    pjsua_callback  cb;		    /**< Application callback.		*/

    /* Media:  */
    pjmedia_endpt   *med_endpt;	    /**< Media endpoint.		*/
    pjmedia_conf    *mconf;	    /**< Media conference.		*/
    unsigned	     wav_slot;	    /**< WAV player slot in bridge	*/
    pjmedia_port    *file_port;	    /**< WAV player port.		*/


    /* Account: */
    pjsua_acc	     acc[PJSUA_MAX_ACC];    /** Client regs array.	*/


    /* Threading (optional): */
    pj_thread_t	    *threads[8];    /**< Thread instances.		*/
    pj_bool_t	     quit_flag;	    /**< To signal thread to quit.	*/

    /* Transport (UDP): */
    pj_sock_t	     sip_sock;	    /**< SIP UDP socket.		*/
    pj_sockaddr_in   sip_sock_name; /**< Public/STUN UDP socket addr.	*/


    /* PJSUA Calls: */
    int		     call_cnt;	    /**< Number of calls.		*/
    pjsua_call	     calls[PJSUA_MAX_CALLS];	/** Calls array.	*/


    /* SIMPLE and buddy status: */
    int		     buddy_cnt;
    pjsua_buddy	     buddies[PJSUA_MAX_BUDDIES];
};


/** PJSUA instance. */
extern struct pjsua pjsua;



/*****************************************************************************
 * PJSUA API (defined in pjsua_core.c).
 */

/**
 * Initialize pjsua settings with default parameters.
 */
PJ_DECL(void) pjsua_default_config(pjsua_config *cfg);


/**
 * Test configuration.
 */
PJ_DECL(pj_status_t) pjsua_test_config(const pjsua_config *cfg,
				       char *errmsg,
				       int len);


/**
 * Create pjsua application.
 * This initializes pjlib/pjlib-util, and creates memory pool factory to
 * be used by application.
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


/*****************************************************************************
 * PJSUA Call API (defined in pjsua_call.c).
 */

/**
 * Make outgoing call.
 */
PJ_DECL(pj_status_t) pjsua_make_call(int acc_index,
				     const char *cstr_dest_uri,
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
PJ_DECL(void) pjsua_call_xfer(int call_index, const char *dest);


/**
 * Send instant messaging inside INVITE session.
 */
PJ_DECL(void) pjsua_call_send_im(int call_index, const char *text);


/**
 * Send IM typing indication inside INVITE session.
 */
PJ_DECL(void) pjsua_call_typing(int call_index, pj_bool_t is_typing);

/**
 * Terminate all calls.
 */
PJ_DECL(void) pjsua_call_hangup_all(void);


/*****************************************************************************
 * PJSUA Client Registration API (defined in pjsua_reg.c).
 */

/**
 * Update registration or perform unregistration. If renew argument is zero,
 * this will start unregistration process.
 */
PJ_DECL(void) pjsua_regc_update(int acc_index, pj_bool_t renew);




/*****************************************************************************
 * PJSUA Presence (pjsua_pres.c)
 */

/**
 * Refresh both presence client and server subscriptions.
 */
PJ_DECL(void) pjsua_pres_refresh(int acc_index);

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
PJ_DECL(pj_status_t) pjsua_im_send(int acc_index, const char *dst_uri, 
				   const char *text);


/**
 * Send typing indication outside dialog.
 */
PJ_DECL(pj_status_t) pjsua_im_typing(int acc_index, const char *dst_uri, 
				     pj_bool_t is_typing);



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
 * Dump settings.
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

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
#   define PJSUA_MAX_BUDDIES	    32
#endif


/**
 * Max simultaneous calls.
 */
#ifndef PJSUA_MAX_CALLS
#   define PJSUA_MAX_CALLS	    256
#endif


/**
 * Aditional ports to be allocated in the conference ports for non-call
 * streams.
 */
#define PJSUA_CONF_MORE_PORTS	    2


/**
 * Maximum accounts.
 */
#ifndef PJSUA_MAX_ACC
#   define PJSUA_MAX_ACC	    8
#endif


/**
 * Maximum credentials.
 */
#ifndef PJSUA_MAX_CRED
#   define PJSUA_MAX_CRED	    PJSUA_MAX_ACC
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
    int			 acc_index; /**< Account index being used.	    */
    pjmedia_session	*session;   /**< The media session.		    */
    unsigned		 conf_slot; /**< Slot # in conference bridge.	    */
    pjsip_evsub		*xfer_sub;  /**< Xfer server subscription, if this
					 call was triggered by xfer.	    */
    pjmedia_sock_info	 skinfo;    /**< Preallocated media sockets.	    */

    void		*app_data;  /**< Application data.		    */
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
 * Account
 */
struct pjsua_acc
{
    int		     index;	    /**< Index in accounts array.	*/
    pj_str_t	     local_uri;	    /**< Uri in From: header.		*/
    pj_str_t	     user_part;	    /**< User part of local URI.	*/
    pj_str_t	     host_part;	    /**< Host part of local URI.	*/
    pj_str_t	     contact_uri;   /**< Uri in Contact: header.	*/

    pj_str_t	     reg_uri;	    /**< Registrar URI.			*/
    pjsip_regc	    *regc;	    /**< Client registration session.   */
    pj_int32_t	     reg_timeout;   /**< Default timeout.		*/
    pj_timer_entry   reg_timer;	    /**< Registration timer.		*/
    pj_status_t	     reg_last_err;  /**< Last registration error.	*/
    int		     reg_last_code; /**< Last status last register.	*/

    pj_str_t	     proxy;	    /**< Proxy URL.			*/
    pjsip_route_hdr  route_set;	    /**< Route set.			*/

    pj_bool_t	     online_status; /**< Our online status.		*/
    pjsua_srv_pres   pres_srv_list; /**< Server subscription list.	*/

    void	    *app_data;	    /**< Application data.		*/
};


typedef struct pjsua_acc pjsua_acc;


/* PJSUA application variables. */
struct pjsua
{
    /* Control: */
    pj_caching_pool  cp;	    /**< Global pool factory.		*/
    pjsip_endpoint  *endpt;	    /**< Global endpoint.		*/
    pj_pool_t	    *pool;	    /**< pjsua's private pool.		*/
    pjsip_module     mod;	    /**< pjsua's PJSIP module.		*/
    

    /* Media:  */
    int		     start_rtp_port;/**< Start of RTP port to try.	*/
    pjmedia_endpt   *med_endpt;	    /**< Media endpoint.		*/
    unsigned	     clock_rate;    /**< Internal clock rate.		*/
    pjmedia_conf    *mconf;	    /**< Media conference.		*/
    pj_bool_t	     null_audio;    /**< Null audio flag.		*/
    char	    *wav_file;	    /**< WAV file name to play.		*/
    unsigned	     wav_slot;	    /**< WAV player slot in bridge	*/
    pj_bool_t	     auto_play;	    /**< Auto play file for calls?	*/
    pj_bool_t	     auto_loop;	    /**< Auto loop RTP stream?		*/
    pj_bool_t	     auto_conf;	    /**< Auto put to conference?	*/

    /* Codec arguments: */
    int		     codec_cnt;	    /**< Number of --add-codec args.	*/
    pj_str_t	     codec_arg[32]; /**< Array of --add-codec args.	*/
    pj_status_t	    (*codec_deinit[32])(void);	/**< Array of funcs.	*/

    /* User Agent behaviour: */
    int		     auto_answer;   /**< Automatically answer in calls.	*/

    /* Account: */
    int		     acc_cnt;	    /**< Number of client registrations	*/
    pjsua_acc	     acc[PJSUA_MAX_ACC];    /** Client regs array.	*/


    /* Authentication credentials: */

    int		     cred_count;    /**< Number of credentials.		*/
    pjsip_cred_info  cred_info[10]; /**< Array of credentials.		*/


    /* Threading (optional): */
    int		     thread_cnt;    /**< Thread count.			*/
    pj_thread_t	    *threads[8];    /**< Thread instances.		*/
    pj_bool_t	     quit_flag;	    /**< To signal thread to quit.	*/

    /* Transport (UDP): */
    pj_uint16_t	     sip_port;	    /**< SIP signaling port.		*/
    pj_sock_t	     sip_sock;	    /**< SIP UDP socket.		*/
    pj_sockaddr_in   sip_sock_name; /**< Public/STUN UDP socket addr.	*/

    pj_str_t	     outbound_proxy;/**< Outbound proxy.		*/


    /* STUN: */
    pj_str_t	     stun_srv1;
    int		     stun_port1;
    pj_str_t	     stun_srv2;
    int		     stun_port2;


    /* Logging: */    
    int		     log_level;	    /**< Logging verbosity.		*/
    int		     app_log_level; /**< stdout log verbosity.		*/
    unsigned	     log_decor;	    /**< Log decoration.		*/
    char	    *log_filename;  /**< Log filename.			*/


    /* PJSUA Calls: */
    int		     max_calls;	    /**< Max nb of calls.		*/
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
void pjsua_default(void);


/**
 * Display error message for the specified error code.
 */
void pjsua_perror(const char *sender, const char *title, 
		  pj_status_t status);


/**
 * Initialize pjsua application. Application can call this before parsing
 * application settings.
 *
 * This will initialize all libraries, create endpoint instance, and register
 * pjsip modules. Transport will NOT be created however.
 *
 * Application may register module after calling this function.
 */
pj_status_t pjsua_init(void);


/**
 * Start pjsua stack. Application calls this after pjsua settings has been
 * configured.
 *
 * This will start the transport, worker threads (if any), and registration 
 * process, if registration is configured.
 */
pj_status_t pjsua_start(void);


/**
 * Destroy pjsua.
 */
pj_status_t pjsua_destroy(void);


/**
 * Find account for incoming request.
 */
int pjsua_find_account_for_incoming(pjsip_rx_data *rdata);


/**
 * Find account for outgoing request.
 */
int pjsua_find_account_for_outgoing(const pj_str_t *url);


/*****************************************************************************
 * PJSUA Call API (defined in pjsua_call.c).
 */

/**
 * Init pjsua call module.
 */
pj_status_t pjsua_call_init(void);

/**
 * Make outgoing call.
 */
pj_status_t pjsua_make_call(int acc_index,
			    const char *cstr_dest_uri,
			    int *p_call_index);


/**
 * Handle incoming invite request.
 */
pj_bool_t pjsua_call_on_incoming(pjsip_rx_data *rdata);


/**
 * Answer call.
 */
void pjsua_call_answer(int call_index, int code);

/**
 * Hangup call.
 */
void pjsua_call_hangup(int call_index, int code);


/**
 * Put call on-hold.
 */
void pjsua_call_set_hold(int call_index);


/**
 * Send re-INVITE (to release hold).
 */
void pjsua_call_reinvite(int call_index);


/**
 * Transfer call.
 */
void pjsua_call_xfer(int call_index, const char *dest);


/**
 * Send instant messaging inside INVITE session.
 */
void pjsua_call_send_im(int call_index, const char *text);


/**
 * Send IM typing indication inside INVITE session.
 */
void pjsua_call_typing(int call_index, pj_bool_t is_typing);

/**
 * Terminate all calls.
 */
void pjsua_call_hangup_all(void);


/*****************************************************************************
 * PJSUA Client Registration API (defined in pjsua_reg.c).
 */

/**
 * Initialize client registration session.
 *
 * @param app_callback	Optional callback
 */
pj_status_t pjsua_regc_init(int acc_index);

/**
 * Update registration or perform unregistration. If renew argument is zero,
 * this will start unregistration process.
 */
void pjsua_regc_update(int acc_index, pj_bool_t renew);




/*****************************************************************************
 * PJSUA Presence (pjsua_pres.c)
 */

/**
 * Init presence.
 */
pj_status_t pjsua_pres_init();

/**
 * Refresh both presence client and server subscriptions.
 */
void pjsua_pres_refresh(int acc_index);

/**
 * Terminate all subscriptions
 */
void pjsua_pres_shutdown(void);

/**
 * Dump presence subscriptions.
 */
void pjsua_pres_dump(pj_bool_t detail);


/*****************************************************************************
 * PJSUA Instant Messaging (pjsua_im.c)
 */

/**
 * The MESSAGE method (defined in pjsua_im.c)
 */
extern const pjsip_method pjsip_message_method;


/**
 * Init IM module handler to handle incoming MESSAGE outside dialog.
 */
pj_status_t pjsua_im_init();


/**
 * Create Accept header for MESSAGE.
 */
pjsip_accept_hdr* pjsua_im_create_accept(pj_pool_t *pool);

/**
 * Send IM outside dialog.
 */
pj_status_t pjsua_im_send(int acc_index, const char *dst_uri, 
			  const char *text);


/**
 * Send typing indication outside dialog.
 */
pj_status_t pjsua_im_typing(int acc_index, const char *dst_uri, 
			    pj_bool_t is_typing);


/**
 * Private: check if we can accept the message.
 *	    If not, then p_accept header will be filled with a valid
 *	    Accept header.
 */
pj_bool_t pjsua_im_accept_pager(pjsip_rx_data *rdata,
				const pjsip_accept_hdr **p_accept_hdr);

/**
 * Private: process pager message.
 *	    This may trigger pjsua_ui_on_pager() or pjsua_ui_on_typing().
 */
void pjsua_im_process_pager(int call_id, const pj_str_t *from,
			    const pj_str_t *to, pjsip_rx_data *rdata);


/*****************************************************************************
 * User Interface API.
 *
 * The UI API specifies functions that will be called by pjsua upon
 * occurence of various events.
 */

/**
 * Notify UI when invite state has changed.
 */
void pjsua_ui_on_call_state(int call_index, pjsip_event *e);

/**
 * Notify UI when registration status has changed.
 */
void pjsua_ui_on_reg_state(int acc_index);

/**
 * Notify UI on incoming pager (i.e. MESSAGE request).
 * Argument call_index will be -1 if MESSAGE request is not related to an 
 * existing call.
 */
void pjsua_ui_on_pager(int call_index, const pj_str_t *from,
		       const pj_str_t *to, const pj_str_t *txt);


/**
 * Notify UI about typing indication.
 */
void pjsua_ui_on_typing(int call_index, const pj_str_t *from,
		        const pj_str_t *to, pj_bool_t is_typing);


/*****************************************************************************
 * Utilities.
 *
 */

/** String to describe invite session states */
extern const char *pjsua_inv_state_names[];

/**
 * Parse arguments (pjsua_opt.c).
 */
pj_status_t pjsua_parse_args(int argc, char *argv[]);

/**
 * Load settings from a file.
 */
pj_status_t pjsua_load_settings(const char *filename);

/**
 * Dump settings.
 */
int pjsua_dump_settings(char *buf, pj_size_t max);

/**
 * Save settings to a file.
 */
pj_status_t pjsua_save_settings(const char *filename);


/*
 * Verify that valid SIP url is given.
 * @return  PJ_SUCCESS if valid.
 */
pj_status_t pjsua_verify_sip_url(const char *c_url);

/*
 * Dump application states.
 */
void pjsua_dump(pj_bool_t detail);


PJ_END_DECL


#endif	/* __PJSUA_H__ */

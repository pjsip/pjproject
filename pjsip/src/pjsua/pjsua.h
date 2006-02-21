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
#define PJSUA_MAX_BUDDIES   32

/**
 * Max simultaneous calls.
 */
#define PJSUA_MAX_CALLS	    8


/** 
 * Structure to be attached to all dialog. 
 * Given a dialog "dlg", application can retrieve this structure
 * by accessing dlg->mod_data[pjsua.mod.id].
 */
struct pjsua_inv_data
{
    PJ_DECL_LIST_MEMBER(struct pjsua_inv_data);

    pjsip_inv_session	*inv;	    /**< The invite session.		    */
    pjmedia_session	*session;   /**< The media session.		    */
    unsigned		 conf_slot; /**< Slot # in conference bridge.	    */
    unsigned		 call_slot; /**< RTP media index in med_sock_use[]  */
    pjsip_evsub		*xfer_sub;  /**< Xfer server subscription, if this
					 call was triggered by xfer.	    */
};


/**
 * Buddy data.
 */
struct pjsua_buddy
{
    pj_str_t		 uri;	    /**< Buddy URI		        */
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



/* PJSUA application variables. */
struct pjsua
{
    /* Control: */

    pj_caching_pool  cp;	    /**< Global pool factory.		*/
    pjsip_endpoint  *endpt;	    /**< Global endpoint.		*/
    pj_pool_t	    *pool;	    /**< pjsua's private pool.		*/
    pjsip_module     mod;	    /**< pjsua's PJSIP module.		*/
    

    /* Media:  */

    pjmedia_endpt   *med_endpt;	    /**< Media endpoint.		*/
    pjmedia_conf    *mconf;	    /**< Media conference.		*/
    pj_bool_t	     null_audio;    /**< Null audio flag.		*/


    /* Since we support simultaneous calls, we need to have multiple
     * RTP sockets.
     */
    pjmedia_sock_info med_sock_info[PJSUA_MAX_CALLS];
    pj_bool_t	      med_sock_use[PJSUA_MAX_CALLS];

    /* User info: */

    pj_str_t	     local_uri;	    /**< Uri in From: header.		*/
    pj_str_t	     contact_uri;   /**< Uri in Contact: header.	*/

    /* Proxy URLs: */

    pj_str_t	     proxy;
    pj_str_t	     outbound_proxy;
    pjsip_route_hdr  route_set;


    /* Registration: */

    pj_str_t	     registrar_uri;
    pjsip_regc	    *regc;
    pj_int32_t	     reg_timeout;
    pj_timer_entry   regc_timer;
    pj_status_t	     regc_last_err; /**< Last registration error.	*/
    int		     regc_last_code;/**< Last status last register.	*/


    /* Authentication credentials: */

    unsigned	     cred_count;
    pjsip_cred_info  cred_info[4];


    /* Threading (optional): */

    int		     thread_cnt;    /**< Thread count.			*/
    pj_thread_t	    *threads[8];    /**< Thread instances.		*/
    pj_bool_t	     quit_flag;	    /**< To signal thread to quit.	*/

    /* Transport (UDP): */

    pj_uint16_t	     sip_port;	    /**< SIP signaling port.		*/
    pj_sock_t	     sip_sock;	    /**< SIP UDP socket.		*/
    pj_sockaddr_in   sip_sock_name; /**< Public/STUN UDP socket addr.	*/



    /* STUN: */

    pj_str_t	     stun_srv1;
    int		     stun_port1;
    pj_str_t	     stun_srv2;
    int		     stun_port2;


    /* Misc: */
    
    int		     log_level;	    /**< Logging verbosity.		*/
    int		     app_log_level; /**< stdout log verbosity.		*/
    unsigned	     log_decor;	    /**< Log decoration.		*/
    char	    *log_filename;  /**< Log filename.			*/


    /* List of invite sessions: */

    struct pjsua_inv_data inv_list;


    /* SIMPLE and buddy status: */

    pj_bool_t	    online_status;  /**< Out online status.		*/
    pjsua_srv_pres  pres_srv_list;  /**< Server subscription list.	*/

    unsigned	    buddy_cnt;
    pjsua_buddy	    buddies[PJSUA_MAX_BUDDIES];
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


/*****************************************************************************
 * PJSUA Invite session API (defined in pjsua_inv.c).
 */

/**
 * Make outgoing call.
 */
pj_status_t pjsua_invite(const char *cstr_dest_uri,
			 struct pjsua_inv_data **p_inv_data);


/**
 * Handle incoming invite request.
 */
pj_bool_t pjsua_inv_on_incoming(pjsip_rx_data *rdata);


/**
 * Hangup call.
 */
void pjsua_inv_hangup(struct pjsua_inv_data *inv_session, int code);


/**
 * Put call on-hold.
 */
void pjsua_inv_set_hold(struct pjsua_inv_data *inv_session);


/**
 * Send re-INVITE (to release hold).
 */
void pjsua_inv_reinvite(struct pjsua_inv_data *inv_session);


/**
 * Transfer call.
 */
void pjsua_inv_xfer_call(struct pjsua_inv_data *inv_session,
			 const char *dest);


/**
 * Callback to be called by session when invite session's state has changed.
 */
void pjsua_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);


/**
 * Callback to be called by session when outgoing dialog has forked.
 * This function will create a forked dialog.
 */
void pjsua_inv_on_new_session(pjsip_inv_session *inv, pjsip_event *e);


/**
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
void pjsua_inv_on_media_update(pjsip_inv_session *inv, pj_status_t status);

/**
 * Callback called when invite session received new offer.
 */
void pjsua_inv_on_rx_offer( pjsip_inv_session *inv,
			    const pjmedia_sdp_session *offer);

/**
 * Callback to receive transaction state inside invite session or dialog
 * (e.g. REFER, MESSAGE).
 */
void pjsua_inv_on_tsx_state_changed(pjsip_inv_session *inv,
				    pjsip_transaction *tsx,
				    pjsip_event *e);

/**
 * Terminate all calls.
 */
void pjsua_inv_shutdown(void);


/*****************************************************************************
 * PJSUA Client Registration API (defined in pjsua_reg.c).
 */

/**
 * Initialize client registration session.
 *
 * @param app_callback	Optional callback
 */
pj_status_t pjsua_regc_init(void);

/**
 * Update registration or perform unregistration. If renew argument is zero,
 * this will start unregistration process.
 */
void pjsua_regc_update(pj_bool_t renew);




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
void pjsua_pres_refresh(void);

/**
 * Terminate all subscriptions
 */
void pjsua_pres_shutdown(void);

/**
 * Dump presence subscriptions.
 */
void pjsua_pres_dump(void);


/*****************************************************************************
 * User Interface API.
 *
 * The UI API specifies functions that will be called by pjsua upon
 * occurence of various events.
 */

/**
 * Notify UI when invite state has changed.
 */
void pjsua_ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);

/**
 * Notify UI when registration status has changed.
 */
void pjsua_ui_regc_on_state_changed(int code);


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
void pjsua_dump(void);


PJ_END_DECL


#endif	/* __PJSUA_H__ */

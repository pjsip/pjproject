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

/* Include all PJSIP-UA headers */
#include <pjsip_ua.h>

/* Include all PJLIB-UTIL headers. */
#include <pjlib-util.h>

/* Include all PJLIB headers. */
#include <pjlib.h>


PJ_BEGIN_DECL

/* PJSUA application variables. */
struct pjsua
{
    /* Control: */

    pj_caching_pool  cp;	    /**< Global pool factory.		*/
    pjsip_endpoint  *endpt;	    /**< Global endpoint.		*/
    pj_pool_t	    *pool;	    /**< pjsua's private pool.		*/
    pjsip_module     mod;	    /**< pjsua's PJSIP module.		*/
    

    /* Media:  */

    pjmedia_endpt   *med_endpt;    /**< Media endpoint.		*/
    pj_bool_t	     null_audio;
    pjmedia_sock_info med_skinfo;

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


    /* Authentication credentials: */

    int		     cred_count;
    pjsip_cred_info  cred_info[4];


    /* Threading: */

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

};


/** PJSUA instance. */
extern struct pjsua pjsua;


/** 
 * Structure to be attached to all dialog. 
 * Given a dialog "dlg", application can retrieve this structure
 * by accessing dlg->mod_data[pjsua.mod.id].
 */
struct pjsua_inv_data
{
    pjmedia_session *session;
};


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
void pjsua_perror(const char *title, pj_status_t status);


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
			 pjsip_inv_session **p_inv);


/**
 * Handle incoming invite request.
 */
pj_bool_t pjsua_inv_on_incoming(pjsip_rx_data *rdata);


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
 * User Interface API.
 *
 * The UI API specifies functions that will be called by pjsua upon
 * occurence of various events.
 */

/**
 * Notify UI when invite state has changed.
 */
void pjsua_ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);


PJ_END_DECL

#endif	/* __PJSUA_H__ */

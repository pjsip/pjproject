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


/* PJSUA application variables. */
extern struct pjsua
{
    /* Control: */

    pj_caching_pool   cp;	    /**< Global pool factory.		*/
    pjsip_endpoint   *endpt;	    /**< Global endpoint.		*/
    pj_pool_t	     *pool;	    /**< pjsua's private pool.		*/
    pjsip_module      mod;	    /**< pjsua's PJSIP module.		*/
    

    /* User info: */
    pj_str_t	     local_uri;	    /**< Uri in From: header.		*/
    pj_str_t	     contact_uri;   /**< Uri in Contact: header.	*/

    /* Threading: */

    int		     thread_cnt;    /**< Thread count.			*/
    pj_thread_t	    *threads[8];    /**< Thread instances.		*/
    pj_bool_t	     quit_flag;	    /**< To signal thread to quit.	*/

    /* Transport (UDP): */

    pj_uint16_t	     sip_port;	    /**< SIP signaling port.		*/
    pj_sock_t	     sip_sock;	    /**< SIP UDP socket.		*/
    pj_sockaddr_in   sip_sock_name; /**< Public/STUN UDP socket addr.	*/
    pj_sock_t	     rtp_sock;	    /**< RTP socket.			*/
    pj_sockaddr_in   rtp_sock_name; /**< Public/STUN UDP socket addr.	*/
    pj_sock_t	     rtcp_sock;	    /**< RTCP socket.			*/
    pj_sockaddr_in   rtcp_sock_name;/**< Public/STUN UDP socket addr.	*/



    /* STUN: */

    pj_str_t	     stun_srv1;
    int		     stun_port1;
    pj_str_t	     stun_srv2;
    int		     stun_port2;


    /* Misc: */
    
    int		     log_level;	    /**< Logging verbosity.		*/
    int		     app_log_level; /**< stdout log verbosity.		*/
    unsigned	     log_decor;	    /**< Log decoration.		*/

} pjsua;


/*****************************************************************************
 * PJSUA API.
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
 * Initialize pjsua application.
 * This will start the registration process, if registration is configured.
 */
pj_status_t pjsua_init(void);


/**
 * Destroy pjsua.
 */
pj_status_t pjsua_destroy(void);


/**
 * Make outgoing call.
 */
pj_status_t pjsua_invite(const char *cstr_dest_uri,
			 pjsip_inv_session **p_inv);


/*****************************************************************************
 * User Interface API.
 * The UI API specifies functions that will be called by pjsua upon
 * occurence of various events.
 */

/**
 * Notify UI when invite state has changed.
 */
void ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e);


#endif	/* __PJSUA_H__ */

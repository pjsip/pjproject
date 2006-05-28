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
#ifndef __PJSUA_IMP_H__
#define __PJSUA_IMP_H__





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
    unsigned		 index;	    /**< Buddy index.			*/
    pj_str_t		 name;	    /**< Buddy name.			*/
    pj_str_t		 display;   /**< Buddy display name.		*/
    pj_str_t		 host;	    /**< Buddy host.			*/
    unsigned		 port;	    /**< Buddy port.			*/
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

    /* Application callback
    : */
    pjsua_callback  cb;		    /**< Application callback.		*/

    /* Media:  */
    pjmedia_endpt   *med_endpt;	    /**< Media endpoint.		*/
    unsigned	     clock_rate;    /**< Conference bridge's clock rate.*/
    unsigned	     samples_per_frame; /**< Bridge's frame size.	*/
    pjmedia_conf    *mconf;	    /**< Media conference.		*/
    
    pjmedia_snd_port *snd_port;	    /**< Sound device port.		*/
    pjmedia_master_port *master_port; /**< Master port, when no snd dev	*/
    
    unsigned	     player_cnt;    /**< Number of file player.		*/

    /** Array of file players */
    struct {
	unsigned     slot;	    /**< WAV player slot in bridge	*/
	pjmedia_port *port;	    /**< WAV player port.		*/
    } player[32];

    unsigned	     recorder_cnt;  /**< Number of file recorders.	*/

    /** Array of file recorders */
    struct {
	unsigned     slot;	    /**< Slot # in conf bridge.		*/
	pjmedia_port *port;	    /**< The recorder media port.	*/
    } recorder[32];

    /* Account: */
    pjsua_acc	     acc[PJSUA_MAX_ACC];    /** Client regs array.	*/


    /* Threading (optional): */
    pj_thread_t	    *threads[8];    /**< Thread instances.		*/
    pj_bool_t	     quit_flag;	    /**< To signal thread to quit.	*/

    /* Transport (UDP): */
    pj_sock_t	     sip_sock;	    /**< SIP UDP socket.		*/
    pj_sockaddr_in   sip_sock_name; /**< Public/STUN UDP socket addr.	*/


    /* PJSUA Calls: */
    unsigned	     call_cnt;	    /**< Number of calls.		*/
    pjsua_call	     calls[PJSUA_MAX_CALLS];	/** Calls array.	*/


    /* SIMPLE and buddy status: */
    pjsua_buddy	     buddies[PJSUA_MAX_BUDDIES];
};


/** PJSUA instance. */
extern struct pjsua pjsua;



/**
 * Find account for incoming request.
 */
int pjsua_find_account_for_incoming(pjsip_rx_data *rdata);


/**
 * Find account for outgoing request.
 */
int pjsua_find_account_for_outgoing(const pj_str_t *url);


/**
 * Init pjsua call module.
 */
pj_status_t pjsua_call_init(void);


/**
 * Handle incoming invite request.
 */
pj_bool_t pjsua_call_on_incoming(pjsip_rx_data *rdata);


/**
 * Initialize client registration session.
 *
 * @param app_callback	Optional callback
 */
pj_status_t pjsua_regc_init(int acc_index);


/**
 * Init presence.
 */
pj_status_t pjsua_pres_init();


/**
 * Terminate all subscriptions
 */
void pjsua_pres_shutdown(void);

/**
 * Init IM module handler to handle incoming MESSAGE outside dialog.
 */
pj_status_t pjsua_im_init();

/**
 * Create Accept header for MESSAGE.
 */
pjsip_accept_hdr* pjsua_im_create_accept(pj_pool_t *pool);

/**
 * Private: check if we can accept the message.
 *	    If not, then p_accept header will be filled with a valid
 *	    Accept header.
 */
pj_bool_t pjsua_im_accept_pager(pjsip_rx_data *rdata,
				pjsip_accept_hdr **p_accept_hdr);

/**
 * Private: process pager message.
 *	    This may trigger pjsua_ui_on_pager() or pjsua_ui_on_typing().
 */
void pjsua_im_process_pager(int call_id, const pj_str_t *from,
			    const pj_str_t *to, pjsip_rx_data *rdata);



#endif	/* __PJSUA_IMP_H__ */


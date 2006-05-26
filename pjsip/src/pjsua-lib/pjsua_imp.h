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


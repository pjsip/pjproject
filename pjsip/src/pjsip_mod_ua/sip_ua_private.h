/* $Header: /pjproject/pjsip/src/pjsip_mod_ua/sip_ua_private.h 3     3/25/05 12:51p Bennylp $ */
/* 
 * PJSIP - SIP Stack
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __PJSIP_UA_PRIVATE_H__
#define __PJSIP_UA_PRIVATE_H__


/*
 * Internal dialog functions.
 */
pj_status_t pjsip_dlg_init_from_rdata( pjsip_dlg *dlg,
				       pjsip_rx_data *rdata );


void pjsip_dlg_on_tsx_event( pjsip_dlg *dlg, 
			     pjsip_transaction *tsx, 
			     pjsip_event *event);


#endif	/* __PJSIP_UA_PRIVATE_H__ */


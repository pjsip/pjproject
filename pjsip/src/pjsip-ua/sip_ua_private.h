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


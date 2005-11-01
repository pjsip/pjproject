/* $Id$
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


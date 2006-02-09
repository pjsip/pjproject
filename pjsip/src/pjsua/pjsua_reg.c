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
#include "pjsua.h"


/*
 * pjsua_reg.c
 *
 * Client registration handler.
 */

#define THIS_FILE   "pjsua_reg.c"


/*
 * This callback is called by pjsip_regc when outgoing register
 * request has completed.
 */
static void regc_cb(struct pjsip_regc_cbparam *param)
{
    /*
     * Print registration status.
     */
    if (param->code < 0 || param->code >= 300) {
	PJ_LOG(2, (THIS_FILE, "SIP registration failed, status=%d (%s)", 
		   param->code, pjsip_get_status_text(param->code)->ptr));
	pjsua.regc = NULL;

    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {
	PJ_LOG(3, (THIS_FILE, "SIP registration success, status=%d (%s), "
			      "will re-register in %d seconds", 
			      param->code,
			      pjsip_get_status_text(param->code)->ptr,
			      param->expiration));

    } else {
	PJ_LOG(4, (THIS_FILE, "SIP registration updated status=%d", param->code));
    }
}


/*
 * Update registration. If renew is false, then unregistration will be performed.
 */
void pjsua_regc_update(pj_bool_t renew)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    if (renew) {
	PJ_LOG(3,(THIS_FILE, "Performing SIP registration..."));
	status = pjsip_regc_register(pjsua.regc, 1, &tdata);
    } else {
	PJ_LOG(3,(THIS_FILE, "Performing SIP unregistration..."));
	status = pjsip_regc_unregister(pjsua.regc, &tdata);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror("Unable to create REGISTER request", status);
	return;
    }

    pjsip_regc_send( pjsua.regc, tdata );
}

/*
 * Initialize client registration.
 */
pj_status_t pjsua_regc_init(void)
{
    pj_status_t status;

    /* initialize SIP registration if registrar is configured */
    if (pjsua.registrar_uri.slen) {

	status = pjsip_regc_create( pjsua.endpt, NULL, &regc_cb, &pjsua.regc);

	if (status != PJ_SUCCESS) {
	    pjsua_perror("Unable to create client registration", status);
	    return status;
	}


	status = pjsip_regc_init( pjsua.regc, &pjsua.registrar_uri, 
				  &pjsua.local_uri, 
				  &pjsua.local_uri,
				  1, &pjsua.contact_uri, 
				  pjsua.reg_timeout);
	if (status != PJ_SUCCESS) {
	    pjsua_perror("Client registration initialization error", status);
	    return status;
	}

	pjsip_regc_set_credentials( pjsua.regc, pjsua.cred_count, 
				    pjsua.cred_info );

	pjsip_regc_set_route_set( pjsua.regc, &pjsua.route_set );
    }

    return PJ_SUCCESS;
}


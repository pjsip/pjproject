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
#include <pjsua-lib/pjsua.h>
#include "pjsua_imp.h"


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

    pjsua_acc *acc = param->token;

    /*
     * Print registration status.
     */
    if (param->status!=PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "SIP registration error", 
		     param->status);
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;
	
    } else if (param->code < 0 || param->code >= 300) {
	PJ_LOG(2, (THIS_FILE, "SIP registration failed, status=%d (%s)", 
		   param->code, 
		   pjsip_get_status_text(param->code)->ptr));
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;

    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

	if (param->expiration < 1) {
	    pjsip_regc_destroy(acc->regc);
	    acc->regc = NULL;
	    PJ_LOG(3,(THIS_FILE, "%s: unregistration success",
		      pjsua.config.acc_config[acc->index].id.ptr));
	} else {
	    PJ_LOG(3, (THIS_FILE, 
		       "%s: registration success, status=%d (%s), "
		       "will re-register in %d seconds", 
		       pjsua.config.acc_config[acc->index].id.ptr,
		       param->code,
		       pjsip_get_status_text(param->code)->ptr,
		       param->expiration));
	}

    } else {
	PJ_LOG(4, (THIS_FILE, "SIP registration updated status=%d", param->code));
    }

    acc->reg_last_err = param->status;
    acc->reg_last_code = param->code;

    if (pjsua.cb.on_reg_state)
	(*pjsua.cb.on_reg_state)(acc->index);
}


/**
 * Get number of accounts.
 */
PJ_DEF(unsigned) pjsua_get_acc_count(void)
{
    return pjsua.config.acc_cnt;
}


/**
 * Get account info.
 */
PJ_DEF(pj_status_t) pjsua_acc_get_info( pjsua_acc_id acc_index,
					pjsua_acc_info *info)
{
    pjsua_acc *acc = &pjsua.acc[acc_index];
    pjsua_acc_config *acc_cfg = &pjsua.config.acc_config[acc_index];

    PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);
    
    pj_memset(info, 0, sizeof(pjsua_acc_info));

    PJ_ASSERT_RETURN(acc_index < (int)pjsua.config.acc_cnt, 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua.acc[acc_index].valid, PJ_EINVALIDOP);

    
    info->index = acc_index;
    info->acc_id = acc_cfg->id;
    info->has_registration = (acc->regc != NULL);
    info->online_status = acc->online_status;
    
    if (acc->reg_last_err) {
	info->status = acc->reg_last_err;
	pj_strerror(acc->reg_last_err, info->buf, sizeof(info->buf));
	info->status_text = pj_str(info->buf);
    } else if (acc->reg_last_code) {
	info->status = acc->reg_last_code;
	info->status_text = *pjsip_get_status_text(acc->reg_last_code);
    } else {
	info->status = 0;
	info->status_text = pj_str("In Progress");
    }
    
    if (acc->regc) {
	pjsip_regc_info regc_info;
	pjsip_regc_get_info(acc->regc, &regc_info);
	info->expires = regc_info.next_reg;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_acc_enum_info( pjsua_acc_info info[],
					 unsigned *count )
{
    unsigned i, c;

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua.acc); ++i) {
	if (!pjsua.acc[i].valid)
	    continue;

	pjsua_acc_get_info(i, &info[c]);
	++c;
    }

    *count = c;
    return PJ_SUCCESS;
}


/**
 * Enum accounts id.
 */
PJ_DEF(pj_status_t) pjsua_acc_enum_id( pjsua_acc_id ids[],
				       unsigned *count )
{
    unsigned i, c;

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua.acc); ++i) {
	if (!pjsua.acc[i].valid)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;
    return PJ_SUCCESS;
}


/*
 * Update registration. If renew is false, then unregistration will be performed.
 */
PJ_DECL(pj_status_t) pjsua_acc_set_registration(pjsua_acc_id acc_index, 
						pj_bool_t renew)
{
    pj_status_t status = 0;
    pjsip_tx_data *tdata = 0;

    PJ_ASSERT_RETURN(acc_index < (int)pjsua.config.acc_cnt, 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua.acc[acc_index].valid, PJ_EINVALIDOP);

    if (renew) {
	if (pjsua.acc[acc_index].regc == NULL) {
	    status = pjsua_regc_init(acc_index);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create registration", 
			     status);
		return PJ_EINVALIDOP;
	    }
	}
	if (!pjsua.acc[acc_index].regc)
	    return PJ_EINVALIDOP;

	status = pjsip_regc_register(pjsua.acc[acc_index].regc, 1, 
				     &tdata);

    } else {
	if (pjsua.acc[acc_index].regc == NULL) {
	    PJ_LOG(3,(THIS_FILE, "Currently not registered"));
	    return PJ_EINVALIDOP;
	}
	status = pjsip_regc_unregister(pjsua.acc[acc_index].regc, &tdata);
    }

    if (status == PJ_SUCCESS)
	status = pjsip_regc_send( pjsua.acc[acc_index].regc, tdata );

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send REGISTER", 
		     status);
    } else {
	PJ_LOG(3,(THIS_FILE, "%s sent",
	         (renew? "Registration" : "Unregistration")));
    }

    return status;
}

/*
 * Initialize client registration.
 */
pj_status_t pjsua_regc_init(int acc_index)
{
    pjsua_acc_config *acc_config;
    pj_status_t status;

    acc_config = &pjsua.config.acc_config[acc_index];

    if (acc_config->reg_uri.slen == 0) {
	PJ_LOG(3,(THIS_FILE, "Registrar URI is not specified"));
	return PJ_SUCCESS;
    }

    /* initialize SIP registration if registrar is configured */

    status = pjsip_regc_create( pjsua.endpt, 
				&pjsua.acc[acc_index], 
				&regc_cb, 
				&pjsua.acc[acc_index].regc);

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create client registration", 
		     status);
	return status;
    }


    status = pjsip_regc_init( pjsua.acc[acc_index].regc, 
			      &acc_config->reg_uri, 
			      &acc_config->id, 
			      &acc_config->id,
			      1, &acc_config->contact, 
			      acc_config->reg_timeout);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Client registration initialization error", 
		     status);
	return status;
    }

    if (acc_config->cred_count) {
	pjsip_regc_set_credentials( pjsua.acc[acc_index].regc, 
				    acc_config->cred_count, 
				    acc_config->cred_info );
    }

    pjsip_regc_set_route_set( pjsua.acc[acc_index].regc, 
			      &pjsua.acc[acc_index].route_set );

    return PJ_SUCCESS;
}


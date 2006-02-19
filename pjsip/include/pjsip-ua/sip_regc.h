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
#ifndef __PJSIP_SIP_REG_H__
#define __PJSIP_SIP_REG_H__

/**
 * @file sip_reg.h
 * @brief SIP Registration Client
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_auth.h>
//#include <pjsip/sip_ua.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSUA_REGC SIP Registration Client
 * @ingroup PJSUA
 * @{
 * \brief
 *   API for performing registration for user agent.
 */

/** Typedef for client registration data. */
typedef struct pjsip_regc pjsip_regc;

/** Maximum contacts in registration. */
#define PJSIP_REGC_MAX_CONTACT	10

/** Expiration not specified. */
#define PJSIP_REGC_EXPIRATION_NOT_SPECIFIED	((pj_uint32_t)0xFFFFFFFFUL)

/** Buffer to hold all contacts. */
#define PJSIP_REGC_CONTACT_BUF_SIZE	512

/** Structure to hold parameters when calling application's callback.
 *  The application's callback is called when the client registration process
 *  has finished.
 */
struct pjsip_regc_cbparam
{
    pjsip_regc		*regc;
    void		*token;
    int			 code;
    pj_status_t		 status;
    pj_str_t		 reason;
    pjsip_rx_data	*rdata;
    int			 contact_cnt;
    int			 expiration;
    pjsip_contact_hdr	*contact[PJSIP_REGC_MAX_CONTACT];
};


/** Type declaration for callback to receive registration result. */
typedef void pjsip_regc_cb(struct pjsip_regc_cbparam *param);

/**
 * Client registration information.
 */
struct pjsip_regc_info
{
    pj_str_t	server_uri; /**< Server URI,				    */
    pj_str_t	client_uri; /**< Client URI (From header).		    */
    pj_bool_t	is_busy;    /**< Have pending transaction?		    */
    pj_bool_t	auto_reg;   /**< Will register automatically?		    */
    int		interval;   /**< Registration interval (seconds).	    */
    int		next_reg;   /**< Time until next registration (seconds).    */
};

/**
 * @see pjsip_regc_info
 */
typedef struct pjsip_regc_info pjsip_regc_info;


/**
 * Get the module instance for client registration module.
 *
 * @return	    client registration module.
 */
PJ_DECL(pjsip_module*) pjsip_regc_get_module(void);


/**
 * Create client registration structure.
 *
 * @param endpt	    Endpoint, used to allocate pool from.
 * @param token	    A data to be associated with the client registration struct.
 * @param cb	    Pointer to callback function to receive registration status.
 * @param p_regc    Pointer to receive client registration structure.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_create( pjsip_endpoint *endpt, void *token,
				        pjsip_regc_cb *cb, 
					pjsip_regc **p_regc);


/**
 * Destroy client registration structure. If a registration transaction is
 * in progress, then the structure will be deleted only after a final response
 * has been received, and in this case, the callback won't be called.
 *
 * @param regc	    The client registration structure.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_destroy(pjsip_regc *regc);

/**
 * Get registration info.
 *
 * @param regc	    The client registration structure.
 * @param info	    Client registration info.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_get_info( pjsip_regc *regc,
					  pjsip_regc_info *info );


/**
 * Get the memory pool associated with a registration client handle.
 *
 * @param regc	    The client registration structure.
 * @return pool	    handle.
 */
PJ_DECL(pj_pool_t*) pjsip_regc_get_pool(pjsip_regc *regc);

/**
 * Initialize client registration structure with various information needed to
 * perform the registration.
 *
 * @param regc	    The client registration structure.
 * @param from_url  The person performing the registration, must be a SIP URL type.
 * @param to_url    The address of record for which the registration is targetd, must
 *		    be a SIP/SIPS URL.
 * @param ccnt	    Number of contacts in the array.
 * @param contact   Array of contacts.
 * @param expires   Default expiration interval (in seconds) to be applied for
 *		    contact URL that doesn't have expiration settings. If the
 *		    value PJSIP_REGC_EXPIRATION_NOT_SPECIFIED is given, then 
 *		    no default expiration will be applied.
 * @return	    zero on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_init(pjsip_regc *regc,
				     const pj_str_t *srv_url,
				     const pj_str_t *from_url,
				     const pj_str_t *to_url,
				     int ccnt,
				     const pj_str_t contact[],
				     pj_uint32_t expires);


/**
 * Set authentication credentials to use by this registration.
 *
 * @param regc	    The registration structure.
 * @param count	    Number of credentials in the array.
 * @param cred	    Array of credentials.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_set_credentials( pjsip_regc *regc,
						 int count,
						 const pjsip_cred_info cred[] );

/**
 * Set route set to be used for outgoing requests.
 *
 * @param regc	    The client registration structure.
 * @param route_set List containing Route headers.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_set_route_set(pjsip_regc *regc,
					      const pjsip_route_hdr*route_set);

/**
 * Create REGISTER request for the specified client registration structure.
 *
 * After successfull registration, application can inspect the contacts in
 * the client registration structure to list what contacts are associaciated
 * with the address of record being targeted in the registration.
 *
 * @param regc	    The client registration structure.
 * @param autoreg   If non zero, the library will automatically refresh the
 *		    next registration until application unregister.
 * @param p_tdata   Pointer to receive the REGISTER request.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_register(pjsip_regc *regc, pj_bool_t autoreg,
					 pjsip_tx_data **p_tdata);


/**
 * Create REGISTER request to unregister all contacts from server records.
 *
 * @param regc	    The client registration structure.
 * @param p_tdata   Pointer to receive the REGISTER request.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_unregister(pjsip_regc *regc,
					   pjsip_tx_data **p_tdata);

/**
 * Update Contact details in the client registration structure.
 *
 * @param regc	    The client registration structure.
 * @param ccnt	    Number of contacts.
 * @param contact   Array of contacts.
 * @return	    zero if sucessfull.
 */
PJ_DECL(pj_status_t) pjsip_regc_update_contact( pjsip_regc *regc,
					        int ccnt,
						const pj_str_t contact[] );

/**
 * Update the expires value.
 *
 * @param regc	    The client registration structure.
 * @param expires   The new expires value.
 * @return	    zero on successfull.
 */
PJ_DECL(pj_status_t) pjsip_regc_update_expires( pjsip_regc *regc,
					        pj_uint32_t expires );

/**
 * Sends outgoing REGISTER request.
 * The process will complete asynchronously, and application
 * will be notified via the callback when the process completes.
 *
 * @param regc	    The client registration structure.
 * @param tdata	    Transmit data.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_regc_send(pjsip_regc *regc, pjsip_tx_data *tdata);


PJ_END_DECL

#endif	/* __PJSIP_REG_H__ */

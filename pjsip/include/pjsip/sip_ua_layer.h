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
#ifndef __PJSIP_SIP_UA_LAYER_H__
#define __PJSIP_SIP_UA_LAYER_H__

/**
 * @file sip_ua_layer.h
 * @brief SIP User Agent Layer Module
 */
#include <pjsip/sip_types.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJSUA SIP User Agent Stack
 */

/**
 * @defgroup PJSUA_UA SIP User Agent Module
 * @ingroup PJSUA
 * @{
 * \brief
 *   User Agent manages the interactions between application and SIP dialogs.
 */

/** User agent initialization parameter. */
typedef struct pjsip_ua_init_param
{
    pjsip_dialog* (*on_dlg_forked)(pjsip_dialog *first_set, pjsip_rx_data *res);
} pjsip_ua_init_param;

/**
 * Initialize user agent layer and register it to the specified endpoint.
 *
 * @param endpt		The endpoint where the user agent will be
 *			registered.
 * @param prm		UA initialization parameter.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_ua_init_module(pjsip_endpoint *endpt,
					  const pjsip_ua_init_param *prm);

/**
 * Get the instance of the user agent.
 *
 * @return		The user agent module instance.
 */
PJ_DECL(pjsip_user_agent*) pjsip_ua_instance(void);

/**
 * Destroy the user agent layer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_ua_destroy(void);

/**
 * Dump user agent contents (e.g. all dialogs).
 */
PJ_DEF(void) pjsip_ua_dump(void);

/**
 * Get the endpoint instance of a user agent module.
 *
 * @param ua		The user agent instance.
 *
 * @return		The endpoint instance where the user agent is
 *			registered.
 */
PJ_DECL(pjsip_endpoint*) pjsip_ua_get_endpt(pjsip_user_agent *ua);


/*
 * Internal (called by sip_dialog.c).
 */

PJ_DECL(pj_status_t) pjsip_ua_register_dlg( pjsip_user_agent *ua,
					    pjsip_dialog *dlg );
PJ_DECL(pj_status_t) pjsip_ua_unregister_dlg(pjsip_user_agent *ua,
					     pjsip_dialog *dlg );


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSIP_SIP_UA_LAYER_H__ */


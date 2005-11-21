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
#ifndef __PJSIP_SIP_UA_H__
#define __PJSIP_SIP_UA_H__

/**
 * @file ua.h
 * @brief SIP User Agent Library
 */

#include <pjsip_mod_ua/sip_dialog.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSUA SIP User Agent Stack
 */

/**
 * @defgroup PJSUA_UA SIP User Agent
 * @ingroup PJSUA
 * @{
 * \brief
 *   User Agent manages the interactions between application and SIP dialogs.
 */

typedef struct pjsip_dlg_callback pjsip_dlg_callback;

/**
 * \brief This structure describes a User Agent instance.
 */
struct pjsip_user_agent
{
    pjsip_endpoint     *endpt;
    pj_pool_t	       *pool;
    pj_mutex_t	       *mutex;
    pj_uint32_t		mod_id;
    pj_hash_table_t    *dlg_table;
    pjsip_dlg_callback *dlg_cb;
    pj_list		dlg_list;
};

/**
 * Create a new dialog.
 */
PJ_DECL(pjsip_dlg*) pjsip_ua_create_dialog( pjsip_user_agent *ua,
					       pjsip_role_e role );


/**
 * Destroy dialog.
 */
PJ_DECL(void) pjsip_ua_destroy_dialog( pjsip_dlg *dlg );


/** 
 * Register callback to receive dialog notifications.
 */
PJ_DECL(void) pjsip_ua_set_dialog_callback( pjsip_user_agent *ua, 
					    pjsip_dlg_callback *cb );


/**
 * Get the module interface for the UA module.
 */
PJ_DECL(pjsip_module*) pjsip_ua_get_module(void);


/**
 * Dump user agent state to log file.
 */
PJ_DECL(void) pjsip_ua_dump( pjsip_user_agent *ua );

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_UA_H__ */


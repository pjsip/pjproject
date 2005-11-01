/* $Header: /pjproject/pjsip/src/pjsip_mod_ua/sip_ua.h 6     6/17/05 11:16p Bennylp $ */
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


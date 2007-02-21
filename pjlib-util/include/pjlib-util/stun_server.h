/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_STUN_SERVER_H__
#define __PJ_STUN_SERVER_H__

/**
 * @file stun_server.h
 * @brief STUN server side services.
 */

#include <pjlib-util/stun_msg.h>
#include <pjlib-util/stun_endpoint.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJLIB_UTIL_STUN_SERVER STUN Server Side Services
 * @brief STUN server side services
 * @ingroup PJLIB_UTIL_STUN
 * @{
 */

typedef struct pj_stun_service pj_stun_service;


/**
 * STUN service handler.
 */
typedef struct pj_stun_service_handler
{
    /**
     * The STUN message type.
     */
    int		 msg_type;

    /** 
     * Callback to be called to handle this STUN message type.
     *
     * @param svc   The service.
     * @param msg   The STUN message.
     */
    pj_status_t	(*handle_msg)(pj_stun_service *svc, 
			      void *handle_data,
			      const pj_stun_msg *msg);

} pj_stun_service_handler;


/**
 * Create STUN service.
 */
PJ_DECL(pj_status_t) pj_stun_service_create(pj_pool_t *pool,
					    const char *name,
					    unsigned options,
					    unsigned handler_cnt,
					    pj_stun_service_handler cb[],
					    void *user_data,
					    pj_stun_service **p_svc);

/**
 * Destroy STUN service
 */
PJ_DECL(pj_status_t) pj_stun_service_destroy(pj_stun_service *svc);


/**
 * Get user data associated with the STUN service.
 */
PJ_DECL(void*) pj_stun_service_get_user_data(pj_stun_service *svc);


/**
 * Instruct the STUN service to handle incoming STUN message.
 */
PJ_DECL(pj_status_t) pj_stun_service_handle_msg(pj_stun_service *svc,
						void *handle_data,
						const pj_stun_msg *msg);



/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJ_STUN_SERVER_H__ */


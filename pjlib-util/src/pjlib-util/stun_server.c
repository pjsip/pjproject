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
#include <pjlib-util/stun_server.h>
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


struct pj_stun_service
{
    pj_str_t		     name;
    unsigned		     options;
    void		    *user_data;
    unsigned		     cb_cnt;
    pj_stun_service_handler *cb;
};


/*
 * Create STUN service.
 */
PJ_DEF(pj_status_t) pj_stun_service_create( pj_pool_t *pool,
					    const char *name,
					    unsigned options,
					    unsigned handler_cnt,
					    pj_stun_service_handler cb[],
					    void *user_data,
					    pj_stun_service **p_svc)
{
    pj_stun_service *svc;

    PJ_ASSERT_RETURN(pool && handler_cnt && cb && p_svc, PJ_EINVAL);

    svc = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_service);
    svc->options = options;
    svc->user_data = user_data;

    if (!name) name = "pj_stun_service";

    pj_strdup2_with_null(pool, &svc->name, name);
    
    svc->cb_cnt = handler_cnt;
    svc->cb = pj_pool_calloc(pool, handler_cnt,	
			     sizeof(pj_stun_service_handler));
    pj_memcpy(svc->cb, cb, handler_cnt * sizeof(pj_stun_service_handler));

    *p_svc = svc;

    return PJ_SUCCESS;
}


/*
 * Destroy STUN service
 */
PJ_DEF(pj_status_t) pj_stun_service_destroy(pj_stun_service *svc)
{
    PJ_ASSERT_RETURN(svc, PJ_EINVAL);
    return PJ_SUCCESS;
}


/*
 * Get user data associated with the STUN service.
 */
PJ_DEF(void*) pj_stun_service_get_user_data(pj_stun_service *svc)
{
    PJ_ASSERT_RETURN(svc, NULL);
    return svc->user_data;
}


/*
 * Find handler.
 */
static pj_stun_service_handler *find_handler(pj_stun_service *svc,
					     int msg_type)
{
    unsigned i;

    for (i=0; i<svc->cb_cnt; ++i) {
	if (svc->cb[i].msg_type == msg_type)
	    return &svc->cb[i];
    }

    return NULL;
}


/*
 * Instruct the STUN service to handle incoming STUN message.
 */
PJ_DEF(pj_status_t) pj_stun_service_handle_msg( pj_stun_service *svc,
					        void *handle_data,
						const pj_stun_msg *msg)
{
    pj_stun_service_handler *handler;

    PJ_ASSERT_RETURN(svc && msg, PJ_EINVAL);

    handler = find_handler(svc, msg->hdr.type);
    if (handler == NULL)
	return PJLIB_UTIL_ESTUNNOHANDLER;

    return (*handler->handle_msg)(svc, handle_data, msg);
}


//////////////////////////////////////////////////////////////////////////////




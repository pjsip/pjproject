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
#ifndef __PJ_STUN_ENDPOINT_H__
#define __PJ_STUN_ENDPOINT_H__

/**
 * @file stun_endpoint.h
 * @brief STUN endpoint.
 */

#include <pjlib-util/stun_msg.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJLIB_UTIL_STUN_ENDPOINT STUN Endpoint
 * @brief Management of incoming and outgoing STUN transactions.
 * @ingroup PJLIB_UTIL_STUN
 * @{
 */

/**
 * Opaque declaration for STUN endpoint. STUN endpoint manages client and
 * server STUN transactions, and it needs to be initialized before application
 * can send or receive STUN messages.
 */
typedef struct pj_stun_endpoint
{
    pj_pool_factory	*pf;
    pj_ioqueue_t	*ioqueue;
    pj_timer_heap_t	*timer_heap;
    unsigned		 options;

    unsigned		 rto_msec;

    pj_pool_t		*pool;

} pj_stun_endpoint;



/**
 * Create a STUN endpoint instance.
 */
PJ_DECL(pj_status_t) pj_stun_endpt_create(pj_pool_factory *factory,
					  unsigned options,
					  pj_ioqueue_t *ioqueue,
					  pj_timer_heap_t *timer_heap,
					  pj_stun_endpoint **p_endpt);

/**
 * Destroy STUN endpoint instance.
 */
PJ_DECL(pj_status_t) pj_stun_endpt_destroy(pj_stun_endpoint *endpt);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJ_STUN_ENDPOINT_H__ */


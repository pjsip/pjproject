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
#ifndef __PJNATH_STUN_CONFIG_H__
#define __PJNATH_STUN_CONFIG_H__

/**
 * @file stun_config.h
 * @brief STUN endpoint.
 */

#include <pjnath/stun_msg.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_STUN_ENDPOINT STUN Endpoint
 * @brief Management of incoming and outgoing STUN transactions.
 * @ingroup PJNATH_STUN
 * @{
 */

/**
 * Opaque declaration for STUN endpoint. STUN endpoint manages client and
 * server STUN transactions, and it needs to be initialized before application
 * can send or receive STUN messages.
 */
typedef struct pj_stun_config
{
    /**
     * Pool factory to be used by the STUN endpoint and all objects created
     * that use this STUN endpoint.
     */
    pj_pool_factory	*pf;

    /**
     * Ioqueue used by this endpoint.
     */
    pj_ioqueue_t	*ioqueue;

    /**
     * Timer heap instance used by this endpoint.
     */
    pj_timer_heap_t	*timer_heap;

    /**
     * Internal pool used by this endpoint. This shouldn't be used by
     * application.
     */
    pj_pool_t		*pool;

    /**
     * Options.
     */
    unsigned		 options;

    /**
     * The default initial STUN round-trip time estimation in msecs.
     * The value normally is PJ_STUN_RTO_VALUE.
     */
    unsigned		 rto_msec;

    /**
     * The interval to cache outgoing  STUN response in the STUN session,
     * in miliseconds. 
     *
     * Default 10000 (10 seconds).
     */
    unsigned		 res_cache_msec;

} pj_stun_config;



/**
 * Create a STUN endpoint instance.
 */
PJ_DECL(pj_status_t) pj_stun_config_create(pj_pool_factory *factory,
					   unsigned options,
					   pj_ioqueue_t *ioqueue,
					   pj_timer_heap_t *timer_heap,
					   pj_stun_config **p_endpt);

/**
 * Destroy STUN endpoint instance.
 */
PJ_DECL(pj_status_t) pj_stun_config_destroy(pj_stun_config *endpt);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_STUN_CONFIG_H__ */


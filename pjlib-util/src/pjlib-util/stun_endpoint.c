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
#include <pjlib-util/stun_endpoint.h>
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>


/*
 * Create a STUN endpoint instance.
 */
PJ_DEF(pj_status_t) pj_stun_endpoint_create( pj_pool_factory *factory,
					     unsigned options,
					     pj_ioqueue_t *ioqueue,
					     pj_timer_heap_t *timer_heap,
					     pj_stun_endpoint **p_endpt)
{
    pj_pool_t *pool;
    pj_stun_endpoint *endpt;

    PJ_ASSERT_RETURN(factory && p_endpt, PJ_EINVAL);

    pool = pj_pool_create(factory, "stunendpt", 1000, 1000, NULL);
    if (!pool)
	return PJ_ENOMEM;
    
    endpt = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_endpoint);
    endpt->pool = pool;
    endpt->pf = factory;
    endpt->options = options;
    endpt->ioqueue = ioqueue;
    endpt->timer_heap = timer_heap;
    endpt->rto_msec = PJ_STUN_RTO_VALUE;

    *p_endpt = endpt;

    return PJ_SUCCESS;
}


/*
 * Destroy STUN endpoint instance.
 */
PJ_DEF(pj_status_t) pj_stun_endpoint_destroy(pj_stun_endpoint *endpt)
{
    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    pj_pool_release(endpt->pool);

    return PJ_SUCCESS;
}


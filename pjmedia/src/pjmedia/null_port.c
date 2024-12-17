/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/null_port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define SIGNATURE   PJMEDIA_SIG_PORT_NULL

struct null_port
{
    pjmedia_port     base;
    pj_pool_t       *pool;
};

static pj_status_t null_get_frame(pjmedia_port *this_port, 
                                  pjmedia_frame *frame);
static pj_status_t null_put_frame(pjmedia_port *this_port, 
                                  pjmedia_frame *frame);
static pj_status_t null_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_null_port_create( pj_pool_t *pool_,
                                              unsigned sampling_rate,
                                              unsigned channel_count,
                                              unsigned samples_per_frame,
                                              unsigned bits_per_sample,
                                              pjmedia_port **p_port )
{
    struct null_port *port;
    const pj_str_t name = pj_str("null-port");
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(pool_ && p_port, PJ_EINVAL);

    /* Create own pool */
    pool = pj_pool_create(pool_->factory, name.ptr, 128, 128, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    port = PJ_POOL_ZALLOC_T(pool, struct null_port);
    PJ_ASSERT_ON_FAIL(port, {pj_pool_release(pool); return PJ_ENOMEM;});
    port->pool = pool;

    pjmedia_port_info_init(&port->base.info, &name, SIGNATURE, sampling_rate,
                           channel_count, bits_per_sample, samples_per_frame);

    port->base.get_frame = &null_get_frame;
    port->base.put_frame = &null_put_frame;
    port->base.on_destroy = &null_on_destroy;


    *p_port = &port->base;
    
    return PJ_SUCCESS;
}



/*
 * Put frame to file.
 */
static pj_status_t null_put_frame(pjmedia_port *this_port, 
                                  pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}


/*
 * Get frame from file.
 */
static pj_status_t null_get_frame(pjmedia_port *this_port, 
                                  pjmedia_frame *frame)
{
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
    frame->timestamp.u32.lo += PJMEDIA_PIA_SPF(&this_port->info);
    pjmedia_zero_samples((pj_int16_t*)frame->buf, 
                          PJMEDIA_PIA_SPF(&this_port->info));

    return PJ_SUCCESS;
}


/*
 * Destroy port.
 */
static pj_status_t null_on_destroy(pjmedia_port *this_port)
{
    struct null_port* port = (struct null_port*) this_port;

    if (port->pool)
        pj_pool_safe_release(&port->pool);

    return PJ_SUCCESS;
}

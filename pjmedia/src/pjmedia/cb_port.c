/* $Id: cb_port.c 3664 2011-07-19 03:42:28Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny at prijono.org>
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
#include <pjmedia/cb_port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define SIGNATURE   PJMEDIA_SIG_PORT_CB

struct cb_port {
    pjmedia_port base;

    pj_timestamp timestamp;
    void        *user_data;

    pj_status_t (*cb_get_frame)(
        pjmedia_port *port,
        void *usr_data,
        void *buffer,
        pj_size_t buf_size);

    pj_status_t (*cb_put_frame)(
        pjmedia_port *port,
        void *usr_data,
        const void *buffer,
        pj_size_t buf_size);
};

static pj_status_t cb_port_get_frame(pjmedia_port *this_port, 
                  pjmedia_frame *frame);
static pj_status_t cb_port_put_frame(pjmedia_port *this_port, 
                  pjmedia_frame *frame);
static pj_status_t cb_port_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_cb_port_create( pj_pool_t *pool,
                            unsigned sampling_rate,
                            unsigned channel_count,
                            unsigned samples_per_frame,
                            unsigned bits_per_sample,
                            void *user_data,
                            pj_status_t (*cb_get_frame)(
                                pjmedia_port *port,
                                void *usr_data,
                                void *buffer,
                                pj_size_t buf_size),
                            pj_status_t (*cb_put_frame)(
                                pjmedia_port *port,
                                void *usr_data,
                                const void *buffer,
                                pj_size_t buf_size),
                            pjmedia_port **p_port )
{
    struct cb_port *cbport;
    const pj_str_t name = pj_str("cb-port");

    PJ_ASSERT_RETURN(pool && sampling_rate && channel_count &&
            samples_per_frame && bits_per_sample && p_port &&
            (cb_get_frame || cb_put_frame),
        PJ_EINVAL);

    cbport = PJ_POOL_ZALLOC_T(pool, struct cb_port);
    PJ_ASSERT_RETURN(cbport != NULL, PJ_ENOMEM);

    /* Create the port */
    pjmedia_port_info_init(&cbport->base.info, &name, SIGNATURE, sampling_rate,
        channel_count, bits_per_sample, samples_per_frame);

    cbport->base.get_frame = &cb_port_get_frame;
    cbport->base.put_frame = &cb_port_put_frame;
    cbport->base.on_destroy = &cb_port_on_destroy;

    /* port->timestamp zeroed in ZALLOC */
    cbport->user_data = user_data;
    cbport->cb_get_frame = cb_get_frame;
    cbport->cb_put_frame = cb_put_frame;

    *p_port = &cbport->base;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_cb_port_userdata_get(pjmedia_port *port,
                            void** user_data)
{
    const struct cb_port *cbport;

    PJ_ASSERT_RETURN(port && user_data, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
        PJ_EINVALIDOP);

    cbport = (struct cb_port*) port;
    *user_data = cbport->user_data;
    return PJ_SUCCESS;
}



/*
 * Put frame for application processing.
 */
static pj_status_t cb_port_put_frame(pjmedia_port *this_port, 
                            pjmedia_frame *frame)
{
    const struct cb_port *cbport;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
        PJ_EINVALIDOP);

    cbport = (struct cb_port*) this_port;
    if (cbport->cb_put_frame && (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)) {
        /* TODO: We should process the return code somehow */
        cbport->cb_put_frame(this_port, cbport->user_data, frame->buf, frame->size);
    }

    return PJ_SUCCESS;
}


/*
 * Get frame from application.
 */
static pj_status_t cb_port_get_frame(pjmedia_port *this_port, 
                            pjmedia_frame *frame)
{
    struct cb_port *cbport;
    pj_size_t size;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
        PJ_EINVALIDOP);

    cbport = (struct cb_port*) this_port;
    size = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
    if (cbport->cb_get_frame) {
        pj_status_t ret;

        ret = cbport->cb_get_frame(this_port, cbport->user_data, frame->buf, size);
        if (ret == PJ_SUCCESS) {
            frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame->size = size;
            /* Is this the correct timestamp calculation? */
            frame->timestamp.u64 = cbport->timestamp.u64;
            cbport->timestamp.u64 += PJMEDIA_PIA_SPF(&this_port->info);
        } else {
            frame->type = PJMEDIA_FRAME_TYPE_NONE;
            frame->size = 0;
        }
    }

    return PJ_SUCCESS;
}


/*
 * Destroy port.
 */
static pj_status_t cb_port_on_destroy(pjmedia_port *this_port)
{
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
        PJ_EINVALIDOP);

    /* Destroy signature */
    this_port->info.signature = 0;

    return PJ_SUCCESS;
}

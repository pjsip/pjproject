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
#include <pjmedia/aec_port.h>
#include <pjmedia/aec.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "aec_port.c"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('A', 'E', 'C', ' ')
#define BUF_COUNT   32

struct aec
{
    pjmedia_port     base;
    pjmedia_port    *dn_port;
    pjmedia_aec	    *aec;
};


static pj_status_t aec_put_frame(pjmedia_port *this_port, 
				 const pjmedia_frame *frame);
static pj_status_t aec_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t aec_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_aec_port_create( pj_pool_t *pool,
					     pjmedia_port *dn_port,
					     unsigned tail_ms,
					     pjmedia_port **p_port )
{
    const pj_str_t AEC = { "AEC", 3 };
    struct aec *aec;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(dn_port->info.bits_per_sample==16 && tail_ms, 
		     PJ_EINVAL);

    /* Create the port and the AEC itself */
    aec = pj_pool_zalloc(pool, sizeof(struct aec));
    
    pjmedia_port_info_init(&aec->base.info, &AEC, SIGNATURE,
			   dn_port->info.clock_rate, 
			   dn_port->info.channel_count, 
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    status = pjmedia_aec_create(pool, dn_port->info.clock_rate, 
				dn_port->info.samples_per_frame,
				tail_ms, 0, &aec->aec);
    if (status != PJ_SUCCESS)
	return status;

    /* More init */
    aec->dn_port = dn_port;
    aec->base.get_frame = &aec_get_frame;
    aec->base.put_frame = &aec_put_frame;
    aec->base.on_destroy = &aec_on_destroy;

    /* Done */
    *p_port = &aec->base;

    return PJ_SUCCESS;
}


static pj_status_t aec_put_frame(pjmedia_port *this_port, 
				 const pjmedia_frame *frame)
{
    struct aec *aec = (struct aec*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    if (frame->type == PJMEDIA_FRAME_TYPE_NONE ) {
	return pjmedia_port_put_frame(aec->dn_port, frame);
    }

    PJ_ASSERT_RETURN(frame->size == this_port->info.samples_per_frame * 2,
		     PJ_EINVAL);

    pjmedia_aec_capture(aec->aec, frame->buf, 0);

    return pjmedia_port_put_frame(aec->dn_port, frame);
}


static pj_status_t aec_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct aec *aec = (struct aec*)this_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    status = pjmedia_port_get_frame(aec->dn_port, frame);
    if (status!=PJ_SUCCESS || frame->type!=PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples(frame->buf, this_port->info.samples_per_frame);
    }

    pjmedia_aec_playback(aec->aec, frame->buf);

    return status;
}


static pj_status_t aec_on_destroy(pjmedia_port *this_port)
{
    struct aec *aec = (struct aec*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    pjmedia_aec_destroy(aec->aec);

    return PJ_SUCCESS;
}



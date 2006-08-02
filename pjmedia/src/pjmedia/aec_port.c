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
#include "../pjmedia-codec/speex/speex_echo.h"
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "aec_port.c"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('A', 'E', 'C', ' ')


struct aec_port
{
    pjmedia_port     base;
    pjmedia_port    *dn_port;
    SpeexEchoState  *state;
    pj_int16_t	    *tmp_frame;
    pj_bool_t	     has_frame;
    pj_int16_t	    *last_frame;
};


static pj_status_t aec_put_frame(pjmedia_port *this_port, 
				 const pjmedia_frame *frame);
static pj_status_t aec_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t aec_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_aec_port_create( pj_pool_t *pool,
					     pjmedia_port *dn_port,
					     unsigned tail_length,
					     pjmedia_port **p_port )
{
    const pj_str_t AEC = { "AEC", 3 };
    struct aec_port *aec_port;
    int sampling_rate;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(dn_port->info.bits_per_sample==16 && tail_length, 
		     PJ_EINVAL);

    /* Create and initialize the port */
    aec_port = pj_pool_zalloc(pool, sizeof(struct aec_port));
    
    pjmedia_port_info_init(&aec_port->base.info, &AEC, SIGNATURE,
			   dn_port->info.clock_rate, 
			   dn_port->info.channel_count, 
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    aec_port->state = speex_echo_state_init(dn_port->info.samples_per_frame,
					    tail_length);

    /* Set sampling rate */
    sampling_rate = 0;
    speex_echo_ctl(aec_port->state, SPEEX_ECHO_GET_SAMPLING_RATE, 
		   &sampling_rate);
    sampling_rate = dn_port->info.clock_rate;
    speex_echo_ctl(aec_port->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
		   &sampling_rate);

    /* More init */
    aec_port->dn_port = dn_port;
    aec_port->base.get_frame = &aec_get_frame;
    aec_port->base.put_frame = &aec_put_frame;
    aec_port->base.on_destroy = &aec_on_destroy;

    aec_port->last_frame = pj_pool_zalloc(pool, sizeof(pj_int16_t) *
					    dn_port->info.samples_per_frame);
    aec_port->tmp_frame = pj_pool_zalloc(pool, sizeof(pj_int16_t) *
					    dn_port->info.samples_per_frame);

    /* Done */
    *p_port = &aec_port->base;

    PJ_LOG(4,(THIS_FILE, "AEC created for port %.*s, clock_rate=%d, "
			 "samples per frame=%d, tail length=%d ms", 
			 (int)dn_port->info.name.slen,
			 dn_port->info.name.ptr,
			 dn_port->info.clock_rate,
			 dn_port->info.samples_per_frame,
			 tail_length * 1000 / dn_port->info.clock_rate));
    return PJ_SUCCESS;
}


static pj_status_t aec_put_frame(pjmedia_port *this_port, 
				 const pjmedia_frame *frame)
{
    struct aec_port *aec_port = (struct aec_port*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    if (frame->type == PJMEDIA_FRAME_TYPE_NONE || !aec_port->has_frame) {
	return pjmedia_port_put_frame(aec_port->dn_port, frame);
    }

    PJ_ASSERT_RETURN(frame->size == this_port->info.samples_per_frame * 2,
		     PJ_EINVAL);

    speex_echo_cancel(aec_port->state, 
		      (const spx_int16_t*)frame->buf, 
		      (const spx_int16_t*)aec_port->last_frame,
		      (spx_int16_t*)aec_port->tmp_frame, 
		      NULL);

    pjmedia_copy_samples(frame->buf, aec_port->tmp_frame,
			 this_port->info.samples_per_frame);

    return pjmedia_port_put_frame(aec_port->dn_port, frame);
}


static pj_status_t aec_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct aec_port *aec_port = (struct aec_port*)this_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    status = pjmedia_port_get_frame(aec_port->dn_port, frame);
    if (status==PJ_SUCCESS && frame->type==PJMEDIA_FRAME_TYPE_AUDIO) {
	aec_port->has_frame = PJ_TRUE;
	pjmedia_copy_samples(aec_port->tmp_frame, frame->buf,
			     this_port->info.samples_per_frame);
    } else {
	aec_port->has_frame = PJ_FALSE;	
    }

    return status;
}


static pj_status_t aec_on_destroy(pjmedia_port *this_port)
{
    struct aec_port *aec_port = (struct aec_port*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    speex_echo_state_destroy(aec_port->state);

    return PJ_SUCCESS;
}



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
#include <pjmedia/mem_port.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>


#define THIS_FILE	    "mem_capture.c"

#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('M', 'R', 'e', 'c')
#define BYTES_PER_SAMPLE    2

struct mem_rec
{
    pjmedia_port     base;

    unsigned	     options;

    char	    *buffer;
    pj_size_t	     buf_size;
    char	    *write_pos;
};


static pj_status_t rec_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t rec_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t rec_on_destroy(pjmedia_port *this_port);


PJ_DECL(pj_status_t) pjmedia_mem_capture_create(pj_pool_t *pool,
						void *buffer,
						pj_size_t size,
						unsigned clock_rate,
						unsigned channel_count,
						unsigned samples_per_frame,
						unsigned bits_per_sample,
						unsigned options,
						pjmedia_port **p_port)
{
    struct mem_rec *rec;
    const pj_str_t name = { "memrec", 6 };

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && buffer && size && clock_rate && channel_count &&
		     samples_per_frame && bits_per_sample && p_port,
		     PJ_EINVAL);

    /* Can only support 16bit PCM */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);


    rec = pj_pool_zalloc(pool, sizeof(struct mem_rec));
    PJ_ASSERT_RETURN(rec != NULL, PJ_ENOMEM);

    /* Create the rec */
    pjmedia_port_info_init(&rec->base.info, &name, SIGNATURE,
			   clock_rate, channel_count, bits_per_sample, 
			   samples_per_frame);


    rec->base.put_frame = &rec_put_frame;
    rec->base.get_frame = &rec_get_frame;
    rec->base.on_destroy = &rec_on_destroy;


    /* Save the buffer */
    rec->buffer = rec->write_pos = (char*)buffer;
    rec->buf_size = size;

    /* Options */
    rec->options = options;

    *p_port = &rec->base;

    return PJ_SUCCESS;
}


static pj_status_t rec_put_frame( pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    struct mem_rec *rec;
    char *endpos;
    pj_size_t size_written;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    rec = (struct mem_rec*) this_port;

    size_written = 0;
    endpos = rec->buffer + rec->buf_size;

    while (size_written < frame->size) {
	pj_size_t max;

	max = frame->size - size_written;
	if ((endpos - rec->write_pos) < (int)max)
	    max = endpos - rec->write_pos;

	pj_memcpy(rec->write_pos, ((char*)frame->buf)+size_written, max);
	size_written += max;
	rec->write_pos += max;

	pj_assert(rec->write_pos <= endpos);

	if (rec->write_pos == endpos)
	    rec->write_pos = rec->buffer;
    }
    return PJ_SUCCESS;
}


static pj_status_t rec_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    PJ_UNUSED_ARG(this_port);

    frame->size = 0;
    frame->type = PJMEDIA_FRAME_TYPE_NONE;

    return PJ_SUCCESS;
}


static pj_status_t rec_on_destroy(pjmedia_port *this_port)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(this_port);
    return PJ_SUCCESS;
}




/* $Id$ */
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
#include <pjmedia/mem_port.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE	    "mem_capture.c"

#define SIGNATURE	    PJMEDIA_SIG_PORT_MEM_CAPTURE
#define BYTES_PER_SAMPLE    2

struct mem_rec
{
    pjmedia_port     base;

    unsigned	     options;

    char	    *buffer;
    pj_size_t	     buf_size;
    char	    *write_pos;

    pj_bool_t        eof;
    void	    *user_data;
    pj_status_t    (*cb)(pjmedia_port *port,
			 void *user_data);
    pj_bool_t	     subscribed;
    void	   (*cb2)(pjmedia_port*, void*);
};


static pj_status_t rec_put_frame(pjmedia_port *this_port, 
				 pjmedia_frame *frame);
static pj_status_t rec_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t rec_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_mem_capture_create( pj_pool_t *pool,
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


    rec = PJ_POOL_ZALLOC_T(pool, struct mem_rec);
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


#if !DEPRECATED_FOR_TICKET_2251
/*
 * Register a callback to be called when the file reading has reached the
 * end of buffer.
 */
PJ_DEF(pj_status_t) pjmedia_mem_capture_set_eof_cb( pjmedia_port *port,
				void *user_data,
				pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data))
{
    struct mem_rec *rec;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    PJ_LOG(1, (THIS_FILE, "pjmedia_mem_capture_set_eof_cb() is deprecated. "
    	       "Use pjmedia_mem_capture_set_eof_cb2() instead."));

    rec = (struct mem_rec*) port;
    rec->user_data = user_data;
    rec->cb = cb;

    return PJ_SUCCESS;
}
#endif


/*
 * Register a callback to be called when the file reading has reached the
 * end of buffer.
 */
PJ_DEF(pj_status_t) pjmedia_mem_capture_set_eof_cb2( pjmedia_port *port,
				void *user_data,
				void (*cb)(pjmedia_port *port,
					   void *usr_data))
{
    struct mem_rec *rec;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    rec = (struct mem_rec*) port;
    rec->user_data = user_data;
    rec->cb2 = cb;

    return PJ_SUCCESS;
} 


/* Get current buffer size */
PJ_DEF(pj_size_t) pjmedia_mem_capture_get_size(pjmedia_port *port)
{
    struct mem_rec *rec;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
                     0);

    rec = (struct mem_rec*) port;
    if (rec->eof){
        return rec->buf_size;
    }
    return rec->write_pos - rec->buffer;
}


static pj_status_t rec_on_event(pjmedia_event *event,
                                void *user_data)
{
    struct mem_rec *rec = (struct mem_rec *)user_data;

    if (event->type == PJMEDIA_EVENT_CALLBACK) {
	if (rec->cb2)
	    (*rec->cb2)(&rec->base, rec->user_data);
    }
    
    return PJ_SUCCESS;
}


static pj_status_t rec_put_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct mem_rec *rec;
    char *endpos;
    pj_size_t size_written;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    rec = (struct mem_rec*) this_port;

    if (rec->eof) {
	return PJ_EEOF;
    }
 
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
	
        if (rec->write_pos == endpos) {
	    
	    /* Rewind */
	    rec->write_pos = rec->buffer;
	    
	    /* Call callback, if any */
	    if (rec->cb2) {
	    	if (!rec->subscribed) {
	            pj_status_t status;

	    	    status = pjmedia_event_subscribe(NULL, rec_on_event,
	    				         rec, rec);
	    	    rec->subscribed = (status == PJ_SUCCESS)? PJ_TRUE:
	    			    	PJ_FALSE;
	        }

	    	if (rec->subscribed) {
	    	    pjmedia_event event;

	    	    pjmedia_event_init(&event, PJMEDIA_EVENT_CALLBACK,
	                      	       NULL, rec);
	    	    pjmedia_event_publish(NULL, rec, &event,
	                                  PJMEDIA_EVENT_PUBLISH_POST_EVENT);
	        }

	        return PJ_SUCCESS;

	    } else if (rec->cb) {
		pj_status_t status;
		
		rec->eof = PJ_TRUE;
		status = (*rec->cb)(this_port, rec->user_data);
		if (status != PJ_SUCCESS) {
		    /* Must not access recorder from here on. It may be
		     * destroyed by application.
		     */
                    return status;
		}
		rec->eof = PJ_FALSE;
	    }
        } 
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
    /* Call callback if data was captured
     * and we're not in the callback already.
     */
    struct mem_rec *rec;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
                     PJ_EINVALIDOP);

    rec = (struct mem_rec*) this_port;

    if (rec->subscribed) {
    	pjmedia_event_unsubscribe(NULL, &rec_on_event, rec, rec);
    	rec->subscribed = PJ_FALSE;
    }

    if(rec->cb && PJ_FALSE == rec->eof) {
	rec->eof = PJ_TRUE;
        (*rec->cb)(this_port, rec->user_data);
    }

    return PJ_SUCCESS;
}



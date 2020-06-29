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


#define THIS_FILE	    "mem_player.c"

#define SIGNATURE	    PJMEDIA_SIG_PORT_MEM_PLAYER
#define BYTES_PER_SAMPLE    2

struct mem_player
{
    pjmedia_port     base;

    unsigned	     options;
    pj_timestamp     timestamp;

    char	    *buffer;
    pj_size_t	     buf_size;
    char	    *read_pos;

    pj_bool_t	     eof;
    void	    *user_data;
    pj_status_t    (*cb)(pjmedia_port *port,
			 void *user_data);
    pj_bool_t	     subscribed;
    void	   (*cb2)(pjmedia_port*, void*);
};


static pj_status_t mem_put_frame(pjmedia_port *this_port, 
				 pjmedia_frame *frame);
static pj_status_t mem_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t mem_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_mem_player_create( pj_pool_t *pool,
					       const void *buffer,
					       pj_size_t size,
					       unsigned clock_rate,
					       unsigned channel_count,
					       unsigned samples_per_frame,
					       unsigned bits_per_sample,
					       unsigned options,
					       pjmedia_port **p_port )
{
    struct mem_player *port;
    pj_str_t name = pj_str("memplayer");

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && buffer && size && clock_rate && channel_count &&
		     samples_per_frame && bits_per_sample && p_port,
		     PJ_EINVAL);

    /* Can only support 16bit PCM */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);


    port = PJ_POOL_ZALLOC_T(pool, struct mem_player);
    PJ_ASSERT_RETURN(port != NULL, PJ_ENOMEM);

    /* Create the port */
    pjmedia_port_info_init(&port->base.info, &name, SIGNATURE, clock_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    port->base.put_frame = &mem_put_frame;
    port->base.get_frame = &mem_get_frame;
    port->base.on_destroy = &mem_on_destroy;


    /* Save the buffer */
    port->buffer = port->read_pos = (char*)buffer;
    port->buf_size = size;

    /* Options */
    port->options = options;

    *p_port = &port->base;

    return PJ_SUCCESS;
}


#if !DEPRECATED_FOR_TICKET_2251
/*
 * Register a callback to be called when the file reading has reached the
 * end of buffer.
 */
PJ_DEF(pj_status_t) pjmedia_mem_player_set_eof_cb( pjmedia_port *port,
			       void *user_data,
			       pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data))
{
    struct mem_player *player;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    PJ_LOG(1, (THIS_FILE, "pjmedia_mem_player_set_eof_cb() is deprecated. "
    	       "Use pjmedia_mem_player_set_eof_cb2() instead."));

    player = (struct mem_player*) port;
    player->user_data = user_data;
    player->cb = cb;

    return PJ_SUCCESS;
}
#endif


/*
 * Register a callback to be called when the file reading has reached the
 * end of buffer.
 */
PJ_DEF(pj_status_t) pjmedia_mem_player_set_eof_cb2( pjmedia_port *port,
			       void *user_data,
			       void (*cb)(pjmedia_port *port,
				          void *usr_data))
{
    struct mem_player *player;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    player = (struct mem_player*) port;
    player->user_data = user_data;
    player->cb2 = cb;

    return PJ_SUCCESS;
}


static pj_status_t mem_put_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);

    return PJ_SUCCESS;
}


static pj_status_t player_on_event(pjmedia_event *event,
                                   void *user_data)
{
    struct mem_player *player = (struct mem_player *)user_data;

    if (event->type == PJMEDIA_EVENT_CALLBACK) {
	if (player->cb2)
	    (*player->cb2)(&player->base, player->user_data);
    }
    
    return PJ_SUCCESS;
}


static pj_status_t mem_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct mem_player *player;
    char *endpos;
    pj_size_t size_needed, size_written;
    pj_bool_t delayed_cb = PJ_FALSE;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    player = (struct mem_player*) this_port;

    if (player->eof) {
	pj_status_t status = PJ_SUCCESS;
	pj_bool_t no_loop = (player->options & PJMEDIA_MEM_NO_LOOP);

	/* Call callback, if any */
	if (player->cb2) {
	    if (!player->subscribed) {
		pj_status_t status2;
	    	status2 = pjmedia_event_subscribe(NULL, &player_on_event,
	    				         player, player);
	    	player->subscribed = (status2 == PJ_SUCCESS)? PJ_TRUE:
	    			     PJ_FALSE;
	    }

	    if (player->subscribed && player->eof != 2) {
	    	if (no_loop) {
	    	    pjmedia_event event;

	    	    /* To prevent the callback from being called repeatedly */
	    	    player->eof = 2;

	    	    pjmedia_event_init(&event, PJMEDIA_EVENT_CALLBACK,
	                      	       NULL, player);
	    	    pjmedia_event_publish(NULL, player, &event,
					  PJMEDIA_EVENT_PUBLISH_POST_EVENT);
		    /* Should not access player port after this since
		     * it might have been destroyed by the callback.
		     */
		} else {
		    delayed_cb = PJ_TRUE;
		}
	    }

	    if (no_loop) {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
		return PJ_EEOF;
	    }

	} else if (player->cb) {
	    status = (*player->cb)(this_port, player->user_data);
	}

	/* If callback returns non PJ_SUCCESS or 'no loop' is specified
	 * return immediately (and don't try to access player port since
	 * it might have been destroyed by the callback).
	 */
	if ((status != PJ_SUCCESS) || no_loop)
	{
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    frame->size = 0;
	    return PJ_EEOF;
	}
	
	player->eof = PJ_FALSE;
    }

    size_needed = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
    size_written = 0;
    endpos = player->buffer + player->buf_size;

    while (size_written < size_needed) {
	char *dst = ((char*)frame->buf) + size_written;
	pj_size_t max;
	
	max = size_needed - size_written;
	if (endpos - player->read_pos < (int)max)
	    max = endpos - player->read_pos;

	pj_memcpy(dst, player->read_pos, max);
	size_written += max;
	player->read_pos += max;

	pj_assert(player->read_pos <= endpos);

	if (player->read_pos == endpos) {
	    /* Set EOF flag */
	    player->eof = PJ_TRUE;
	    /* Reset read pointer */
	    player->read_pos = player->buffer;

	    /* Pad with zeroes then return for no looped play */
	    if (player->options & PJMEDIA_MEM_NO_LOOP) {
		pj_size_t null_len;

    		null_len = size_needed - size_written;
		pj_bzero(dst + max, null_len);
		break;
	    }
	}
    }

    frame->size = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
    frame->timestamp.u64 = player->timestamp.u64;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    player->timestamp.u64 += PJMEDIA_PIA_SPF(&this_port->info);

    if (delayed_cb) {
	pjmedia_event event;
	pjmedia_event_init(&event, PJMEDIA_EVENT_CALLBACK,
          		   NULL, player);
	pjmedia_event_publish(NULL, player, &event,
			      PJMEDIA_EVENT_PUBLISH_POST_EVENT);
	/* Should not access player port after this since
	* it might have been destroyed by the callback.
	*/
    }

    return PJ_SUCCESS;
}


static pj_status_t mem_on_destroy(pjmedia_port *this_port)
{
    struct mem_player *player;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
                     PJ_EINVALIDOP);

    player = (struct mem_player*) this_port;

    if (player->subscribed) {
    	pjmedia_event_unsubscribe(NULL, &player_on_event, player, player);
    	player->subscribed = PJ_FALSE;
    }

    /* Destroy signature */
    this_port->info.signature = 0;

    return PJ_SUCCESS;
}



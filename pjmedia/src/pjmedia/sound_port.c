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
#include <pjmedia/sound_port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/string.h>	    /* pj_memset() */

struct pjmedia_snd_port
{
    int			 rec_id;
    int			 play_id;
    pjmedia_snd_stream	*snd_stream;
    pjmedia_dir		 dir;
    pjmedia_port	*port;

    unsigned		 clock_rate;
    unsigned		 channel_count;
    unsigned		 samples_per_frame;
    unsigned		 bits_per_sample;
};

/*
 * The callback called by sound player when it needs more samples to be
 * played.
 */
static pj_status_t play_cb(/* in */   void *user_data,
			   /* in */   pj_uint32_t timestamp,
			   /* out */  void *output,
			   /* out */  unsigned size)
{
    pjmedia_snd_port *snd_port = user_data;
    pjmedia_port *port;
    pjmedia_frame frame;
    pj_status_t status;

    /* We're risking accessing the port without holding any mutex.
     * It's possible that port is disconnected then destroyed while
     * we're trying to access it.
     * But in the name of performance, we'll try this approach until
     * someone complains when it crashes.
     */
    port = snd_port->port;
    if (port == NULL)
	goto no_frame;

    frame.buf = output;
    frame.size = size;
    frame.timestamp.u32.lo = timestamp;

    status = pjmedia_port_get_frame(port, &frame);
    if (status != PJ_SUCCESS)
	goto no_frame;

    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
	goto no_frame;

    return PJ_SUCCESS;

no_frame:
    pj_memset(output, 0, size);
    return PJ_SUCCESS;
}


/*
 * The callback called by sound recorder when it has finished capturing a
 * frame.
 */
static pj_status_t rec_cb(/* in */   void *user_data,
			  /* in */   pj_uint32_t timestamp,
			  /* in */   const void *input,
			  /* in*/    unsigned size)
{
    pjmedia_snd_port *snd_port = user_data;
    pjmedia_port *port;
    pjmedia_frame frame;

    /* We're risking accessing the port without holding any mutex.
     * It's possible that port is disconnected then destroyed while
     * we're trying to access it.
     * But in the name of performance, we'll try this approach until
     * someone complains when it crashes.
     */
    port = snd_port->port;
    if (port == NULL)
	return PJ_SUCCESS;

    frame.buf = (void*)input;
    frame.size = size;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.timestamp.u32.lo = timestamp;

    pjmedia_port_put_frame(port, &frame);

    return PJ_SUCCESS;
}

/*
 * Start the sound stream.
 * This may be called even when the sound stream has already been started.
 */
static pj_status_t start_sound_device( pjmedia_snd_port *snd_port )
{
    pj_status_t status;

    /* Check if sound has been started. */
    if (snd_port->snd_stream != NULL)
	return PJ_SUCCESS;

    /* Open sound stream. */
    if (snd_port->dir == PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_snd_open_rec( snd_port->rec_id, 
				       snd_port->clock_rate,
				       snd_port->channel_count,
				       snd_port->samples_per_frame,
				       snd_port->bits_per_sample,
				       &rec_cb,
				       snd_port,
				       &snd_port->snd_stream);

    } else if (snd_port->dir == PJMEDIA_DIR_PLAYBACK) {
	status = pjmedia_snd_open_player( snd_port->play_id, 
					  snd_port->clock_rate,
					  snd_port->channel_count,
					  snd_port->samples_per_frame,
					  snd_port->bits_per_sample,
					  &play_cb,
					  snd_port,
					  &snd_port->snd_stream);

    } else if (snd_port->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	status = pjmedia_snd_open( snd_port->rec_id, 
				   snd_port->play_id,
				   snd_port->clock_rate,
				   snd_port->channel_count,
				   snd_port->samples_per_frame,
				   snd_port->bits_per_sample,
				   &rec_cb,
				   &play_cb,
				   snd_port,
				   &snd_port->snd_stream);
    } else {
	pj_assert(!"Invalid dir");
	status = PJ_EBUG;
    }

    if (status != PJ_SUCCESS)
	return status;

    /* Start sound stream. */
    status = pjmedia_snd_stream_start(snd_port->snd_stream);
    if (status != PJ_SUCCESS) {
	pjmedia_snd_stream_close(snd_port->snd_stream);
	snd_port->snd_stream = NULL;
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Stop the sound device.
 * This may be called even when there's no sound device in the port.
 */
static pj_status_t stop_sound_device( pjmedia_snd_port *snd_port )
{
    /* Check if we have sound stream device. */
    if (snd_port->snd_stream) {
	pjmedia_snd_stream_stop(snd_port->snd_stream);
	pjmedia_snd_stream_close(snd_port->snd_stream);
	snd_port->snd_stream = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Create bidirectional port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create( pj_pool_t *pool,
					     int rec_id,
					     int play_id,
					     unsigned clock_rate,
					     unsigned channel_count,
					     unsigned samples_per_frame,
					     unsigned bits_per_sample,
					     unsigned options,
					     pjmedia_snd_port **p_port)
{
    pjmedia_snd_port *snd_port;

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    snd_port = pj_pool_zalloc(pool, sizeof(pjmedia_snd_port));
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->rec_id = rec_id;
    snd_port->play_id = play_id;
    snd_port->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;

    *p_port = snd_port;

    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( snd_port );

}

/*
 * Create sound recorder port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create_rec( pj_pool_t *pool,
						 int dev_id,
						 unsigned clock_rate,
						 unsigned channel_count,
						 unsigned samples_per_frame,
						 unsigned bits_per_sample,
						 unsigned options,
						 pjmedia_snd_port **p_port)
{
    pjmedia_snd_port *snd_port;

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    snd_port = pj_pool_zalloc(pool, sizeof(pjmedia_snd_port));
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->rec_id = dev_id;
    snd_port->dir = PJMEDIA_DIR_CAPTURE;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;

    *p_port = snd_port;

    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( snd_port );
}


/*
 * Create sound player port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_create_player( pj_pool_t *pool,
						    int dev_id,
						    unsigned clock_rate,
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned bits_per_sample,
						    unsigned options,
						    pjmedia_snd_port **p_port)
{
    pjmedia_snd_port *snd_port;

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    snd_port = pj_pool_zalloc(pool, sizeof(pjmedia_snd_port));
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->play_id = dev_id;
    snd_port->dir = PJMEDIA_DIR_PLAYBACK;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;

    *p_port = snd_port;

    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( snd_port );
}


/*
 * Destroy port (also destroys the sound device).
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_destroy(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, PJ_EINVAL);

    return stop_sound_device(snd_port);
}


/*
 * Retrieve the sound stream associated by this sound device port.
 */
PJ_DEF(pjmedia_snd_stream*) pjmedia_snd_port_get_snd_stream(
						pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, NULL);
    return snd_port->snd_stream;
}


/*
 * Connect a port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_connect( pjmedia_snd_port *snd_port,
					      pjmedia_port *port)
{
    pjmedia_port_info *pinfo;

    PJ_ASSERT_RETURN(snd_port && port, PJ_EINVAL);

    /* Check that port has the same configuration as the sound device
     * port.
     */
    pinfo = &port->info;
    if (pinfo->sample_rate != snd_port->clock_rate)
	return PJMEDIA_ENCCLOCKRATE;

    if (pinfo->samples_per_frame != snd_port->samples_per_frame)
	return PJMEDIA_ENCSAMPLESPFRAME;

    if (pinfo->channel_count != snd_port->channel_count)
	return PJMEDIA_ENCCHANNEL;

    if (pinfo->bits_per_sample != snd_port->bits_per_sample)
	return PJMEDIA_ENCBITS;

    /* Port is okay. */
    snd_port->port = port;
    return PJ_SUCCESS;
}


/*
 * Get the connected port.
 */
PJ_DEF(pjmedia_port*) pjmedia_snd_port_get_port(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, NULL);
    return snd_port->port;
}


/*
 * Disconnect port.
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_disconnect(pjmedia_snd_port *snd_port)
{
    PJ_ASSERT_RETURN(snd_port, PJ_EINVAL);

    snd_port->port = NULL;

    return PJ_SUCCESS;
}



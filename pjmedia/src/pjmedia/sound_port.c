/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/delaybuf.h>
#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pjmedia/plc.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>	    /* pj_memset() */

//#define SIMULATE_LOST_PCT   20
#define AEC_TAIL	    128	    /* default AEC length in ms */
#define AEC_SUSPEND_LIMIT   5	    /* seconds of no activity	*/

#define THIS_FILE	    "sound_port.c"

#define TEST_OVERFLOW_UNDERFLOW

enum
{
    PJMEDIA_PLC_ENABLED	    = 1,
};

//#define DEFAULT_OPTIONS	PJMEDIA_PLC_ENABLED
#define DEFAULT_OPTIONS	    0


struct pjmedia_snd_port
{
    int			 rec_id;
    int			 play_id;
    pjmedia_snd_stream	*snd_stream;
    pjmedia_dir		 dir;
    pjmedia_port	*port;
    unsigned		 options;

    pjmedia_echo_state	*ec_state;
    unsigned		 aec_tail_len;

    pj_bool_t		 ec_suspended;
    unsigned		 ec_suspend_count;
    unsigned		 ec_suspend_limit;

    pjmedia_plc		*plc;

    unsigned		 clock_rate;
    unsigned		 channel_count;
    unsigned		 samples_per_frame;
    unsigned		 bits_per_sample;

#if PJMEDIA_SOUND_USE_DELAYBUF
    pjmedia_delay_buf	*delay_buf;
#endif
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
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
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
    frame.timestamp.u32.hi = 0;
    frame.timestamp.u32.lo = timestamp;

#if PJMEDIA_SOUND_USE_DELAYBUF
    if (snd_port->delay_buf) {
	status = pjmedia_delay_buf_get(snd_port->delay_buf, (pj_int16_t*)output);
	if (status != PJ_SUCCESS)
	    pj_bzero(output, size);

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	pjmedia_port_put_frame(port, &frame);

#ifdef TEST_OVERFLOW_UNDERFLOW
	{
	    static int count = 1;
	    if (++count % 10 == 0) {
		status = pjmedia_delay_buf_get(snd_port->delay_buf, 
					       (pj_int16_t*)output);
		if (status != PJ_SUCCESS)
		    pj_bzero(output, size);

		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		pjmedia_port_put_frame(port, &frame);
	    }
	}
#endif

    }
#endif

    status = pjmedia_port_get_frame(port, &frame);
    if (status != PJ_SUCCESS)
	goto no_frame;

    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
	goto no_frame;

    /* Must supply the required samples */
    pj_assert(frame.size == size);

#ifdef SIMULATE_LOST_PCT
    /* Simulate packet lost */
    if (pj_rand() % 100 < SIMULATE_LOST_PCT) {
	PJ_LOG(4,(THIS_FILE, "Frame dropped"));
	goto no_frame;
    }
#endif

    if (snd_port->plc)
	pjmedia_plc_save(snd_port->plc, (pj_int16_t*) output);

    if (snd_port->ec_state) {
	if (snd_port->ec_suspended) {
	    snd_port->ec_suspended = PJ_FALSE;
	    //pjmedia_echo_state_reset(snd_port->ec_state);
	    PJ_LOG(4,(THIS_FILE, "EC activated"));
	}
	snd_port->ec_suspend_count = 0;
	pjmedia_echo_playback(snd_port->ec_state, (pj_int16_t*)output);
    }


    return PJ_SUCCESS;

no_frame:

    if (snd_port->ec_state && !snd_port->ec_suspended) {
	++snd_port->ec_suspend_count;
	if (snd_port->ec_suspend_count > snd_port->ec_suspend_limit) {
	    snd_port->ec_suspended = PJ_TRUE;
	    PJ_LOG(4,(THIS_FILE, "EC suspended because of inactivity"));
	}
	if (snd_port->ec_state) {
	    /* To maintain correct delay in EC */
	    pjmedia_echo_playback(snd_port->ec_state, (pj_int16_t*)output);
	}
    }

    /* Apply PLC */
    if (snd_port->plc) {

	pjmedia_plc_generate(snd_port->plc, (pj_int16_t*) output);
#ifdef SIMULATE_LOST_PCT
	PJ_LOG(4,(THIS_FILE, "Lost frame generated"));
#endif
    } else {
	pj_bzero(output, size);
    }


    return PJ_SUCCESS;
}


/*
 * The callback called by sound recorder when it has finished capturing a
 * frame.
 */
static pj_status_t rec_cb(/* in */   void *user_data,
			  /* in */   pj_uint32_t timestamp,
			  /* in */   void *input,
			  /* in*/    unsigned size)
{
    pjmedia_snd_port *snd_port = (pjmedia_snd_port*) user_data;
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

    /* Cancel echo */
    if (snd_port->ec_state && !snd_port->ec_suspended) {
	pjmedia_echo_capture(snd_port->ec_state, (pj_int16_t*) input, 0);
    }

#if PJMEDIA_SOUND_USE_DELAYBUF
    if (snd_port->delay_buf) {
	pjmedia_delay_buf_put(snd_port->delay_buf, (pj_int16_t*)input);
    } else {
	frame.buf = (void*)input;
	frame.size = size;
	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.timestamp.u32.lo = timestamp;

	pjmedia_port_put_frame(port, &frame);
    }
#else
    frame.buf = (void*)input;
    frame.size = size;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.timestamp.u32.lo = timestamp;

    pjmedia_port_put_frame(port, &frame);
#endif

    return PJ_SUCCESS;
}

/*
 * Start the sound stream.
 * This may be called even when the sound stream has already been started.
 */
static pj_status_t start_sound_device( pj_pool_t *pool,
				       pjmedia_snd_port *snd_port )
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


#ifdef SIMULATE_LOST_PCT
    snd_port->options |= PJMEDIA_PLC_ENABLED;
#endif

    /* If we have player components, allocate buffer to save the last
     * frame played to the speaker. The last frame is used for packet
     * lost concealment (PLC) algorithm.
     */
    if ((snd_port->dir & PJMEDIA_DIR_PLAYBACK) &&
	(snd_port->options & PJMEDIA_PLC_ENABLED)) 
    {
	status = pjmedia_plc_create(pool, snd_port->clock_rate, 
				    snd_port->samples_per_frame * 
					snd_port->channel_count,
				    0, &snd_port->plc);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(4,(THIS_FILE, "Unable to create PLC"));
	    snd_port->plc = NULL;
	}
    }

    /* Inactivity limit before EC is suspended. */
    snd_port->ec_suspend_limit = AEC_SUSPEND_LIMIT *
				 (snd_port->clock_rate / 
				  snd_port->samples_per_frame);

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

    /* Destroy AEC */
    if (snd_port->ec_state) {
	pjmedia_echo_destroy(snd_port->ec_state);
	snd_port->ec_state = NULL;
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
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);

    snd_port = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_port);
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->rec_id = rec_id;
    snd_port->play_id = play_id;
    snd_port->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    snd_port->options = options | DEFAULT_OPTIONS;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;
    
#if PJMEDIA_SOUND_USE_DELAYBUF
    status = pjmedia_delay_buf_create(pool, "snd_buff", 
				      clock_rate, samples_per_frame, 
				      PJMEDIA_SOUND_BUFFER_COUNT, -1, 
				      &snd_port->delay_buf);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#else
    PJ_UNUSED_ARG(status);
#endif

    *p_port = snd_port;


    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( pool, snd_port );

}

/*
 * Create sound recorder AEC.
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

    snd_port = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_port);
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->rec_id = dev_id;
    snd_port->dir = PJMEDIA_DIR_CAPTURE;
    snd_port->options = options | DEFAULT_OPTIONS;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;

    *p_port = snd_port;

    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( pool, snd_port );
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

    snd_port = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_port);
    PJ_ASSERT_RETURN(snd_port, PJ_ENOMEM);

    snd_port->play_id = dev_id;
    snd_port->dir = PJMEDIA_DIR_PLAYBACK;
    snd_port->options = options | DEFAULT_OPTIONS;
    snd_port->clock_rate = clock_rate;
    snd_port->channel_count = channel_count;
    snd_port->samples_per_frame = samples_per_frame;
    snd_port->bits_per_sample = bits_per_sample;

    *p_port = snd_port;

    /* Start sound device immediately.
     * If there's no port connected, the sound callback will return
     * empty signal.
     */
    return start_sound_device( pool, snd_port );
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
 * Enable AEC
 */
PJ_DEF(pj_status_t) pjmedia_snd_port_set_ec( pjmedia_snd_port *snd_port,
					     pj_pool_t *pool,
					     unsigned tail_ms,
					     unsigned options)
{
    pjmedia_snd_stream_info si;
    pj_status_t status;

    /* Sound must be opened in full-duplex mode */
    PJ_ASSERT_RETURN(snd_port && 
		     snd_port->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK,
		     PJ_EINVALIDOP);

    /* Sound port must have 16bits per sample */
    PJ_ASSERT_RETURN(snd_port->bits_per_sample == 16,
		     PJ_EINVALIDOP);

    /* Destroy AEC */
    if (snd_port->ec_state) {
	pjmedia_echo_destroy(snd_port->ec_state);
	snd_port->ec_state = NULL;
    }

    snd_port->aec_tail_len = tail_ms;

    if (tail_ms != 0) {
	unsigned delay_ms;

	status = pjmedia_snd_stream_get_info(snd_port->snd_stream, &si);
	if (status != PJ_SUCCESS)
	    si.rec_latency = si.play_latency = 0;

	delay_ms = (si.rec_latency + si.play_latency) * 1000 /
		   snd_port->clock_rate;
	status = pjmedia_echo_create(pool, snd_port->clock_rate, 
				    snd_port->samples_per_frame, 
				    tail_ms, delay_ms,
				    options, &snd_port->ec_state);
	if (status != PJ_SUCCESS)
	    snd_port->ec_state = NULL;
	else
	    snd_port->ec_suspended = PJ_FALSE;
    } else {
	PJ_LOG(4,(THIS_FILE, "Echo canceller is now disabled in the "
			     "sound port"));
	status = PJ_SUCCESS;
    }

    return status;
}


/* Get AEC tail length */
PJ_DEF(pj_status_t) pjmedia_snd_port_get_ec_tail( pjmedia_snd_port *snd_port,
						  unsigned *p_length)
{
    PJ_ASSERT_RETURN(snd_port && p_length, PJ_EINVAL);
    *p_length =  snd_port->ec_state ? snd_port->aec_tail_len : 0;
    return PJ_SUCCESS;
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
    if (pinfo->clock_rate != snd_port->clock_rate)
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



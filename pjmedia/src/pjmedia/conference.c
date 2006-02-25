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
#include <pjmedia/conference.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/stream.h>
#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   printf x
#else
#   define TRACE_(x)
#endif


#define THIS_FILE	"conference.c"
#define RX_BUF_COUNT	8

/*
 * DON'T GET CONFUSED!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. 
 */


struct conf_port
{
    pj_str_t		 name;		/**< Port name.			    */
    pjmedia_port	*port;		/**< get_frame() and put_frame()    */
    pjmedia_port_op	 rx_setting;	/**< Can we receive from this port  */
    pjmedia_port_op	 tx_setting;	/**< Can we transmit to this port   */
    int			 listener_cnt;	/**< Number of listeners.	    */
    pj_bool_t		*listeners;	/**< Array of listeners.	    */
    pjmedia_silence_det	*vad;		/**< VAD for this port.		    */

    /* Tx buffer contains the frame to be "transmitted" to this port
     * (i.e. for put_frame()).
     * We use dual buffer since the port may be accessed by two threads,
     * and we don't want to use mutex for synchronization.
     */
    pj_int16_t		*cur_tx_buf;	/**< Buffer for put_frame().	    */
    pj_int16_t		*tx_buf1;	/**< Buffer 1.			    */
    pj_int16_t		*tx_buf2;	/**< Buffer 2.			    */

    /* Rx buffers is a special buffer for sound device port (port 0). 
     * It's not used by other ports.
     */
    int			 rx_write, rx_read;
    pj_int16_t		*rx_buf[RX_BUF_COUNT];	/**< Buffer 			    */


    /* Sum buf is a temporary buffer used to calculate the average signal
     * received by this port from all other ports.
     */
    unsigned		 sources;	/**< Number of sources.		    */
    pj_uint32_t		*sum_buf;	/**< Total sum of signal.	    */
};


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    unsigned		  max_ports;	/**< Maximum ports.		    */
    unsigned		  port_cnt;	/**< Current number of ports.	    */
    unsigned		  connect_cnt;	/**< Total number of connections    */
    pj_snd_stream	 *snd_rec;	/**< Sound recorder stream.	    */
    pj_snd_stream	 *snd_player;	/**< Sound player stream.	    */
    pj_mutex_t		 *mutex;	/**< Conference mutex.		    */
    struct conf_port	**ports;	/**< Array of ports.		    */
    pj_uint16_t		 *uns_buf;	/**< Buf for unsigned conversion    */
    unsigned		  sampling_rate;	/**< Sampling rate.	    */
    unsigned		  samples_per_frame;	/**< Samples per frame.	    */
    unsigned		  bits_per_sample;	/**< Bits per sample.	    */
    pj_snd_stream_info	  snd_info;
};


/* Extern */
unsigned char linear2ulaw(int pcm_val);

/* Prototypes */
static pj_status_t play_cb( /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* out */ void *output,
			    /* out */ unsigned size);
static pj_status_t rec_cb(  /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* in */  const void *input,
			    /* in*/   unsigned size);

/*
 * Create port.
 */
static pj_status_t create_conf_port( pj_pool_t *pool,
				     pjmedia_conf *conf,
				     const pj_str_t *name,
				     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pj_status_t status;

    /* Create port. */
    conf_port = pj_pool_zalloc(pool, sizeof(struct conf_port));
    PJ_ASSERT_RETURN(conf_port, PJ_ENOMEM);

    /* Set name */
    pj_strdup(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Create transmit flag array */
    conf_port->listeners = pj_pool_zalloc(pool, 
					  conf->max_ports*sizeof(pj_bool_t));
    PJ_ASSERT_RETURN(conf_port->listeners, PJ_ENOMEM);


    /* Create and init vad. */
    status = pjmedia_silence_det_create( pool, &conf_port->vad);
    if (status != PJ_SUCCESS)
	return status;

    pjmedia_silence_det_set_adaptive(conf_port->vad, conf->samples_per_frame);


    /* Create TX buffers. */
    conf_port->tx_buf1 = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->tx_buf1[0]));
    PJ_ASSERT_RETURN(conf_port->tx_buf1, PJ_ENOMEM);

    conf_port->tx_buf2 = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->tx_buf2[0]));
    PJ_ASSERT_RETURN(conf_port->tx_buf2, PJ_ENOMEM);

    /* Set initial TX buffer */
    conf_port->cur_tx_buf = conf_port->tx_buf1;

    /* Create temporary buffer to calculate average signal received by
     * this port.
     */
    conf_port->sum_buf = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->sum_buf[0]));

    

    /* Done */
    *p_conf_port = conf_port;
    return PJ_SUCCESS;
}

/*
 * Create port zero for the sound device.
 */
static pj_status_t create_sound_port( pj_pool_t *pool,
				      pjmedia_conf *conf )
{
    struct conf_port *conf_port;
    pj_str_t name = { "sound-device", 12 };
    unsigned i;
    pj_status_t status;


    /* Init default sound device parameters. */
    pj_memset(&conf->snd_info, 0, sizeof(conf->snd_info));
    conf->snd_info.samples_per_sec = conf->sampling_rate;
    conf->snd_info.bits_per_sample = conf->bits_per_sample;
    conf->snd_info.samples_per_frame = conf->samples_per_frame;
    conf->snd_info.bytes_per_frame = conf->samples_per_frame * 
			       conf->bits_per_sample / 8;
    conf->snd_info.frames_per_packet = 1;
    

    /* Create port */
    status = create_conf_port(pool, conf, &name, &conf_port);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Sound device has rx buffers. */
    for (i=0; i<RX_BUF_COUNT; ++i) {
	conf_port->rx_buf[i] = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->rx_buf[0][0]));
	if (conf_port->rx_buf[i] == NULL) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}
    }
    conf_port->rx_write = 0;
    conf_port->rx_read = 0;


     /* Set to port zero */
    conf->ports[0] = conf_port;
    conf->port_cnt++;

    PJ_LOG(5,(THIS_FILE, "Sound device successfully created for port 0"));
    return PJ_SUCCESS;

on_error:
    return status;

}

/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					 unsigned max_ports,
					 unsigned sampling_rate,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 pjmedia_conf **p_conf )
{
    pjmedia_conf *conf;
    pj_status_t status;

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
	      max_ports));

    /* Create and init conf structure. */
    conf = pj_pool_zalloc(pool, sizeof(pjmedia_conf));
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);

    conf->ports = pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->max_ports = max_ports;
    conf->sampling_rate = sampling_rate;
    conf->samples_per_frame = samples_per_frame;
    conf->bits_per_sample = bits_per_sample;


    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS)
	return status;

    /* Create temporary buffer. */
    conf->uns_buf = pj_pool_zalloc(pool, samples_per_frame *
					 sizeof(conf->uns_buf[0]));

    /* Create mutex. */
    status = pj_mutex_create_simple(pool, "conf", &conf->mutex);
    if (status != PJ_SUCCESS)
	return status;


    /* Done */

    *p_conf = conf;

    return PJ_SUCCESS;
}


/*
 * Create sound device
 */
static pj_status_t create_sound( pjmedia_conf *conf )
{
    /* Open recorder. */
    conf->snd_rec = pj_snd_open_recorder(-1 ,&conf->snd_info, &rec_cb, conf);
    if (conf->snd_rec == NULL) {
	return -1;
    }

    /* Open player */
    conf->snd_player = pj_snd_open_player(-1, &conf->snd_info, &play_cb, conf);
    if (conf->snd_player == NULL) {
	pj_snd_stream_close(conf->snd_rec);
	return -1;
    }

    return PJ_SUCCESS;
}

/*
 * Destroy sound device
 */
static pj_status_t destroy_sound( pjmedia_conf *conf )
{
    if (conf->snd_rec) {
	pj_snd_stream_close(conf->snd_rec);
	conf->snd_rec = NULL;
    }
    if (conf->snd_player) {
	pj_snd_stream_close(conf->snd_player);
	conf->snd_player = NULL;
    }
    return PJ_SUCCESS;
}

/*
 * Activate sound device.
 */
static pj_status_t resume_sound( pjmedia_conf *conf )
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_status_t status;

    if (conf->snd_rec == NULL) {
	status = create_sound(conf);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Start recorder. */
    if (conf->snd_rec) {
	status = pj_snd_stream_start(conf->snd_rec);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Start player. */
    if (conf->snd_player) {
	status = pj_snd_stream_start(conf->snd_player);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(THIS_FILE, "Error starting sound player/recorder: %s",
	      errmsg));
    return status;
}


/*
 * Suspend sound device
 */
static void suspend_sound( pjmedia_conf *conf )
{
    if (conf->snd_rec)
	pj_snd_stream_stop(conf->snd_rec);
    if (conf->snd_player)
	pj_snd_stream_stop(conf->snd_player);
}


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    suspend_sound(conf);
    destroy_sound(conf);
    pj_mutex_destroy(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Add stream port to the conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
					   pj_pool_t *pool,
					   pjmedia_port *strm_port,
					   const pj_str_t *port_name,
					   unsigned *p_port )
{
    struct conf_port *conf_port;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(conf && pool && strm_port && port_name && p_port, 
		     PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    if (conf->port_cnt >= conf->max_ports) {
	pj_assert(!"Too many ports");
	pj_mutex_unlock(conf->mutex);
	return PJ_ETOOMANY;
    }

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
	if (conf->ports[index] == NULL)
	    break;
    }

    pj_assert(index != conf->max_ports);

    /* Create port structure. */
    status = create_conf_port(pool, conf, port_name, &conf_port);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(conf->mutex);
	return status;
    }

    /* Set the port */
    conf_port->port = strm_port;

    /* Put the port. */
    conf->ports[index] = conf_port;
    conf->port_cnt++;

    /* Done. */
    *p_port = index;

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Change TX and RX settings for the port.
 */
PJ_DECL(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
						  unsigned slot,
						  pjmedia_port_op tx,
						  pjmedia_port_op rx)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[slot] != NULL, PJ_EINVAL);

    conf_port = conf->ports[slot];

    if (tx != PJMEDIA_PORT_NO_CHANGE)
	conf_port->tx_setting = tx;

    if (rx != PJMEDIA_PORT_NO_CHANGE)
	conf_port->rx_setting = rx;

    return PJ_SUCCESS;
}


/*
 * Connect port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
					       unsigned src_slot,
					       unsigned sink_slot )
{
    struct conf_port *src_port, *dst_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Ports must be valid. */
    PJ_ASSERT_RETURN(conf->ports[src_slot] != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(conf->ports[sink_slot] != NULL, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    if (src_port->listeners[sink_slot] == 0) {
	src_port->listeners[sink_slot] = 1;
	++conf->connect_cnt;
	++src_port->listener_cnt;

	if (conf->connect_cnt == 1)
	    resume_sound(conf);

	PJ_LOG(4,(THIS_FILE,"Port %.*s transmitting to port %.*s",
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Disconnect port
 */
PJ_DEF(pj_status_t) pjmedia_conf_disconnect_port( pjmedia_conf *conf,
						  unsigned src_slot,
						  unsigned sink_slot )
{
    struct conf_port *src_port, *dst_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Ports must be valid. */
    PJ_ASSERT_RETURN(conf->ports[src_slot] != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(conf->ports[sink_slot] != NULL, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    if (src_port->listeners[sink_slot] != 0) {
	src_port->listeners[sink_slot] = 0;
	--conf->connect_cnt;
	--src_port->listener_cnt;

	PJ_LOG(4,(THIS_FILE,"Port %.*s stop transmitting to port %.*s",
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));

	if (conf->connect_cnt == 0) {
	    suspend_sound(conf);
	    destroy_sound(conf);
	}
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Remove the specified port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
					      unsigned port )
{
    struct conf_port *conf_port;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[port] != NULL, PJ_EINVAL);

    /* Suspend the sound devices.
     * Don't want to remove port while port is being accessed by sound
     * device's threads!
     */

    pj_mutex_lock(conf->mutex);

    conf_port = conf->ports[port];
    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;

    /* Remove this port from transmit array of other ports. */
    for (i=0; i<conf->max_ports; ++i) {
	conf_port = conf->ports[i];

	if (!conf_port)
	    continue;

	if (conf_port->listeners[port] != 0) {
	    --conf->connect_cnt;
	    --conf_port->listener_cnt;
	    conf_port->listeners[port] = 0;
	}
    }

    /* Remove all ports listening from this port. */
    conf_port = conf->ports[port];
    for (i=0; i<conf->max_ports; ++i) {
	if (conf_port->listeners[i]) {
	    --conf->connect_cnt;
	    --conf_port->listener_cnt;
	}
    }

    /* Remove the port. */
    conf->ports[port] = NULL;
    --conf->port_cnt;

    /* Stop sound if there's no connection. */
    if (conf->connect_cnt == 0) {
	destroy_sound(conf);
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Get port info
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_port_info( pjmedia_conf *conf,
						unsigned slot,
						pjmedia_conf_port_info *info)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[slot] != NULL, PJ_EINVAL);

    conf_port = conf->ports[slot];

    info->slot = slot;
    info->name = conf_port->name;
    info->tx_setting = conf_port->tx_setting;
    info->rx_setting = conf_port->rx_setting;
    info->listener = conf_port->listeners;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_conf_get_ports_info(pjmedia_conf *conf,
						unsigned *size,
						pjmedia_conf_port_info info[])
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && size && info, PJ_EINVAL);

    for (i=0; i<conf->max_ports && count<*size; ++i) {
	if (!conf->ports[i])
	    continue;

	pjmedia_conf_get_port_info(conf, i, &info[count]);
	++count;
    }

    *size = count;
    return PJ_SUCCESS;
}


/* Convert signed 16bit pcm sample to unsigned 16bit sample */
static pj_uint16_t pcm2unsigned(pj_int32_t pcm)
{
    return (pj_uint16_t)(pcm + 32767);
}

/* Convert unsigned 16bit sample to signed 16bit pcm sample */
static pj_int16_t unsigned2pcm(pj_uint32_t uns)
{
    return (pj_int16_t)(uns - 32767);
}

/*
 * Player callback.
 */
static pj_status_t play_cb( /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* out */ void *output,
			    /* out */ unsigned size)
{
    pjmedia_conf *conf = user_data;
    pj_int16_t *output_buf = output;
    unsigned ci, cj, i, j;
    
    PJ_UNUSED_ARG(timestamp);
    PJ_UNUSED_ARG(size);

    TRACE_(("p"));

    pj_mutex_lock(conf->mutex);

    /* Zero all port's temporary buffers. */
    for (i=0, ci=0; i<conf->max_ports && ci < conf->port_cnt; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pj_uint32_t *sum_buf;

	/* Skip empty slot. */
	if (!conf_port)
	    continue;

	++ci;

	conf_port->sources = 0;
	sum_buf = conf_port->sum_buf;

	for (j=0; j<conf->samples_per_frame; ++j)
	    sum_buf[j] = 0;
    }

    /* Get frames from all ports, and "mix" the signal 
     * to sum_buf of all listeners of the port.
     */
    for (i=0, ci=0; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pj_int32_t level;

	/* Skip empty port. */
	if (!conf_port)
	    continue;

	++ci;

	/* Skip if we're not allowed to receive from this port. */
	if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
	    continue;
	}

	/* Get frame from this port. 
	 * For port zero (sound port), get the frame  from the rx_buffer
	 * instead.
	 */
	if (i==0) {
	    pj_int16_t *rx_buf;

	    if (conf_port->rx_read == conf_port->rx_write) {
		conf_port->rx_read = 
		    (conf_port->rx_write+RX_BUF_COUNT-RX_BUF_COUNT/2) % 
			RX_BUF_COUNT;
	    }

	    /* Skip if this port is muted/disabled. */
	    if (conf_port->rx_setting != PJMEDIA_PORT_ENABLE) {
		continue;
	    }


	    /* Skip if no port is listening to the microphone */
	    if (conf_port->listener_cnt == 0) {
		continue;
	    }

	    rx_buf = conf_port->rx_buf[conf_port->rx_read];
	    for (j=0; j<conf->samples_per_frame; ++j) {
		((pj_int16_t*)output)[j] = rx_buf[j];
	    }
	    conf_port->rx_read = (conf_port->rx_read+1) % RX_BUF_COUNT;

	} else {
	    pjmedia_frame frame;

	    pj_memset(&frame, 0, sizeof(frame));
	    frame.buf = output;
	    frame.size = size;
	    pjmedia_port_get_frame(conf_port->port, &frame);

	   if (frame.type == PJMEDIA_FRAME_TYPE_NONE)
		continue;
	}

	/* Skip (after receiving the frame) if this port is muted. */
	if (conf_port->rx_setting == PJMEDIA_PORT_MUTE)
	    continue;

	/* Also skip if this port doesn't have listeners. */
	if (conf_port->listener_cnt == 0)
	    continue;

	/* Get the signal level. */
	level = pjmedia_calc_avg_signal(output, conf->samples_per_frame);

	/* Convert level to 8bit complement ulaw */
	level = linear2ulaw(level) ^ 0xff;

	/* Convert the buffer to unsigned 16bit value */
	for (j=0; j<conf->samples_per_frame; ++j)
	    conf->uns_buf[j] = pcm2unsigned(((pj_int16_t*)output)[j]);

	/* Add the signal to all listeners. */
	for (j=0, cj=0; 
	     j<conf->max_ports && cj<(unsigned)conf_port->listener_cnt;
	     ++j) 
	{
	    struct conf_port *listener = conf->ports[j];
	    pj_uint32_t *sum_buf;
	    unsigned k;

	    if (listener == 0)
		continue;

	    /* Skip if this is not the listener. */
	    if (!conf_port->listeners[j])
		continue;

	    ++cj;

	    /* Skip if this listener doesn't want to receive audio */
	    if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
		continue;

	    /* Mix the buffer */
	    sum_buf = listener->sum_buf;
	    for (k=0; k<conf->samples_per_frame; ++k)
		sum_buf[k] += (conf->uns_buf[k] * level);

	    listener->sources += level;
	}
    }

    /* For all ports, calculate avg signal. */
    for (i=0, ci=0; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pjmedia_frame frame;
	pj_int16_t *target_buf;

	if (!conf_port)
	    continue;

	++ci;

	if (conf_port->tx_setting == PJMEDIA_PORT_MUTE) {
	    frame.type = PJMEDIA_FRAME_TYPE_NONE;
	    frame.buf = NULL;
	    frame.size = 0;

	    if (conf_port->port)
		pjmedia_port_put_frame(conf_port->port, &frame);

	    continue;

	} else if (conf_port->tx_setting != PJMEDIA_PORT_ENABLE) {
	    continue;
	}

	target_buf = (conf_port->cur_tx_buf==conf_port->tx_buf1?
			conf_port->tx_buf2 : conf_port->tx_buf1);

	if (conf_port->sources) {
	    for (j=0; j<conf->samples_per_frame; ++j) {
		target_buf[j] = unsigned2pcm(conf_port->sum_buf[j] / 
					     conf_port->sources);
	    }
	}

	/* Switch buffer. */
	conf_port->cur_tx_buf = target_buf;

	pj_memset(&frame, 0, sizeof(frame));
	if (conf_port->sources) {

	    pj_bool_t is_silence = PJ_FALSE;

	    /* Apply silence detection. */
#if 0
	    is_silence = pjmedia_silence_det_detect(conf_port->vad,
						    target_buf,
						    conf->samples_per_frame,
						    NULL);
#endif
	    frame.type = is_silence ? PJMEDIA_FRAME_TYPE_NONE : 
				      PJMEDIA_FRAME_TYPE_AUDIO;

	} else
	    frame.type = PJMEDIA_FRAME_TYPE_NONE;

	frame.buf = conf_port->cur_tx_buf;
	frame.size = conf->samples_per_frame * conf->bits_per_sample / 8;
	frame.timestamp.u64 = timestamp;

	if (conf_port->port)
	    pjmedia_port_put_frame(conf_port->port, &frame);

    }

    /* Return sound playback frame. */
    if (conf->ports[0]->sources) {
	for (j=0; j<conf->samples_per_frame; ++j)
	    output_buf[j] = conf->ports[0]->cur_tx_buf[j];
    } else {
	for (j=0; j<conf->samples_per_frame; ++j)
	    output_buf[j] = 0;
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Recorder callback.
 */
static pj_status_t rec_cb(  /* in */  void *user_data,
			    /* in */  pj_uint32_t timestamp,
			    /* in */  const void *input,
			    /* in */  unsigned size)
{
    pjmedia_conf *conf = user_data;
    struct conf_port *snd_port = conf->ports[0];
    pj_int16_t *target_rx_buf;
    unsigned i;

    PJ_UNUSED_ARG(timestamp);
    
    TRACE_(("r"));

    if (size != conf->samples_per_frame*2) {
	TRACE_(("rxerr "));
    }

    /* Skip if this port is muted/disabled. */
    if (snd_port->rx_setting != PJMEDIA_PORT_ENABLE)
	return PJ_SUCCESS;

    /* Skip if no port is listening to the microphone */
    if (snd_port->listener_cnt == 0)
	return PJ_SUCCESS;


    /* Determine which rx_buffer to fill in */
    target_rx_buf = snd_port->rx_buf[snd_port->rx_write];
    
    /* Copy samples from audio device to target rx_buffer */
    for (i=0; i<conf->samples_per_frame; ++i) {
	target_rx_buf[i] = ((pj_int16_t*)input)[i];
    }

    /* Switch buffer */
    snd_port->rx_write = (snd_port->rx_write+1)%RX_BUF_COUNT;


    /* Time for all ports (except sound port) to transmit frames */
    /*
    for (i=1; i<conf->max_ports; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pjmedia_frame frame;

	if (!conf_port)
	    continue;

    }
    */
    
    return PJ_SUCCESS;
}


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
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/resample.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/sound.h>
#include <pjmedia/stream.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   {printf x; fflush(stdout); }
#else
#   define TRACE_(x)
#endif


#define THIS_FILE	"conference.c"
#define RX_BUF_COUNT	8

#define BYTES_PER_SAMPLE    2

/*
 * DON'T GET CONFUSED!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. 
 */


/**
 * This is a port connected to conference bridge.
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

    /* Shortcut for port info. */
    unsigned		 clock_rate;	/**< Port's clock rate.		    */
    unsigned		 samples_per_frame; /**< Port's samples per frame.  */

    /* Resample, for converting clock rate, if they're different. */
    pjmedia_resample	*rx_resample;
    pjmedia_resample	*tx_resample;

    /* RX buffer is temporary buffer to be used when there is mismatch
     * between port's sample rate or ptime with conference's sample rate
     * or ptime. When both sample rate and ptime of the port match the
     * conference settings, this buffer will not be used.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t		*rx_buf;	/**< The RX buffer.		    */
    unsigned		 rx_buf_cap;	/**< Max size, in samples	    */
    unsigned		 rx_buf_count;	/**< # of samples in the buf.	    */

    /* Mix buf is a temporary buffer used to calculate the average signal
     * received by this port from all other ports.
     *
     * This buffer contains samples at bridge's clock rate.
     * The size of this buffer is equal to samples per frame of the bridge.
     *
     * Note that the samples here are unsigned 32bit.
     */
    unsigned		 sources;	/**< Number of sources.		    */
    pj_uint32_t		*mix_buf;	/**< Total sum of signal.	    */

    /* Tx buffer is a temporary buffer to be used when there's mismatch 
     * between port's clock rate or ptime with conference's sample rate
     * or ptime. When both sample rate and ptime of the port match the
     * conference's settings, this buffer will not be used.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t		*tx_buf;	/**< Tx buffer.			    */
    unsigned		 tx_buf_cap;	/**< Max size, in samples.	    */
    unsigned		 tx_buf_count;	/**< # of samples in the buffer.    */

    /* Snd buffers is a special buffer for sound device port (port 0). 
     * It's not used by other ports.
     *
     * There are multiple numbers of this buffer, because we can not expect
     * the mic and speaker thread to run equally after one another. In most
     * systems, each thread will run multiple times before the other thread
     * gains execution time. For example, in my system, mic thread is called
     * three times, then speaker thread is called three times, and so on.
     */
    int			 snd_write_pos, snd_read_pos;
    pj_int16_t		*snd_buf[RX_BUF_COUNT];	/**< Buffer 		    */
};


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    unsigned		  options;	/**< Bitmask options.		    */
    unsigned		  max_ports;	/**< Maximum ports.		    */
    unsigned		  port_cnt;	/**< Current number of ports.	    */
    unsigned		  connect_cnt;	/**< Total number of connections    */
    pj_snd_stream	 *snd_rec;	/**< Sound recorder stream.	    */
    pj_snd_stream	 *snd_player;	/**< Sound player stream.	    */
    pj_mutex_t		 *mutex;	/**< Conference mutex.		    */
    struct conf_port	**ports;	/**< Array of ports.		    */
    pj_uint16_t		 *uns_buf;	/**< Buf for unsigned conversion    */
    unsigned		  clock_rate;	/**< Sampling rate.	    */
    unsigned		  samples_per_frame;	/**< Samples per frame.	    */
    unsigned		  bits_per_sample;	/**< Bits per sample.	    */
    pj_snd_stream_info	  snd_info;	/**< Sound device parameter.	    */
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
				     pjmedia_port *port,
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

    /* Save some port's infos, for convenience. */
    if (port) {
	conf_port->port = port;
	conf_port->clock_rate = port->info.sample_rate;
	conf_port->samples_per_frame = port->info.samples_per_frame;
    } else {
	conf_port->port = NULL;
	conf_port->clock_rate = conf->clock_rate;
	conf_port->samples_per_frame = conf->samples_per_frame;
    }

    /* If port's clock rate is different than conference's clock rate,
     * create a resample sessions.
     */
    if (conf_port->clock_rate != conf->clock_rate) {

	double factor;

	factor = 1.0 * conf_port->clock_rate / conf->clock_rate;

	/* Create resample for rx buffer. */
	status = pjmedia_resample_create( pool, 
					  PJ_TRUE,  /* High quality */
					  PJ_TRUE,  /* Large filter */
					  conf_port->clock_rate,/* Rate in */
					  conf->clock_rate, /* Rate out */
					  (unsigned)(conf->samples_per_frame * 
						     factor),
					  &conf_port->rx_resample);
	if (status != PJ_SUCCESS)
	    return status;


	/* Create resample for tx buffer. */
	status = pjmedia_resample_create(pool,
					 PJ_TRUE,   /* High quality */
					 PJ_TRUE,   /* Large filter */
					 conf->clock_rate,  /* Rate in */
					 conf_port->clock_rate, /* Rate out */
					 conf->samples_per_frame,
					 &conf_port->tx_resample);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /*
     * Initialize rx and tx buffer, only when port's samples per frame or 
     * port's clock rate is different then the conference bridge settings.
     */
    if (conf_port->clock_rate != conf->clock_rate ||
	conf_port->samples_per_frame != conf->samples_per_frame)
    {
	/* Create RX buffer. */
	conf_port->rx_buf_cap = (unsigned)(conf_port->samples_per_frame +
					   conf->samples_per_frame * 
					   conf_port->clock_rate * 1.0 /
					   conf->clock_rate);
	conf_port->rx_buf_count = 0;
	conf_port->rx_buf = pj_pool_alloc(pool, conf_port->rx_buf_cap *
						sizeof(conf_port->rx_buf[0]));
	PJ_ASSERT_RETURN(conf_port->rx_buf, PJ_ENOMEM);

	/* Create TX buffer. */
	conf_port->tx_buf_cap = conf_port->rx_buf_cap;
	conf_port->tx_buf_count = 0;
	conf_port->tx_buf = pj_pool_alloc(pool, conf_port->tx_buf_cap *
						sizeof(conf_port->tx_buf[0]));
	PJ_ASSERT_RETURN(conf_port->tx_buf, PJ_ENOMEM);
    }


    /* Create mix buffer. */
    conf_port->mix_buf = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->mix_buf[0]));
    PJ_ASSERT_RETURN(conf_port->mix_buf, PJ_ENOMEM);


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
    conf->snd_info.samples_per_sec = conf->clock_rate;
    conf->snd_info.bits_per_sample = conf->bits_per_sample;
    conf->snd_info.samples_per_frame = conf->samples_per_frame;
    conf->snd_info.bytes_per_frame = conf->samples_per_frame * 
			       conf->bits_per_sample / 8;
    conf->snd_info.frames_per_packet = 1;
    

    /* Create port */
    status = create_conf_port(pool, conf, NULL, &name, &conf_port);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Sound device has rx buffers. */
    for (i=0; i<RX_BUF_COUNT; ++i) {
	conf_port->snd_buf[i] = pj_pool_zalloc(pool, conf->samples_per_frame *
					      sizeof(conf_port->snd_buf[0][0]));
	if (conf_port->snd_buf[i] == NULL) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}
    }
    conf_port->snd_write_pos = 0;
    conf_port->snd_read_pos = 0;


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
					 unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 unsigned options,
					 pjmedia_conf **p_conf )
{
    pjmedia_conf *conf;
    pj_status_t status;

    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
	      max_ports));

    /* Create and init conf structure. */
    conf = pj_pool_zalloc(pool, sizeof(pjmedia_conf));
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);

    conf->ports = pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->options = options;
    conf->max_ports = max_ports;
    conf->clock_rate = clock_rate;
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
    status = pj_mutex_create_recursive(pool, "conf", &conf->mutex);
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
    /* Open recorder only if mic is not disabled. */
    if ((conf->options & PJMEDIA_CONF_NO_MIC) == 0) {
	conf->snd_rec = pj_snd_open_recorder(-1 ,&conf->snd_info, 
					     &rec_cb, conf);
	if (conf->snd_rec == NULL) {
	    return -1;
	}
    }

    /* Open player */
    conf->snd_player = pj_snd_open_player(-1, &conf->snd_info, &play_cb, conf);
    if (conf->snd_player == NULL) {
	if (conf->snd_rec) {
	    pj_snd_stream_close(conf->snd_rec);
	}
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

    if (conf->snd_player == NULL) {
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


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    //suspend_sound(conf);
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
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(conf->mutex);
	return status;
    }

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
    pj_bool_t start_sound = PJ_FALSE;

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
	    start_sound = 1;

	PJ_LOG(4,(THIS_FILE,"Port %.*s transmitting to port %.*s",
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));
    }

    pj_mutex_unlock(conf->mutex);

    /* Sound device must be started without mutex, otherwise the
     * sound thread will deadlock (?)
     */
    if (start_sound)
	resume_sound(conf);

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

	
    }

    pj_mutex_unlock(conf->mutex);

    if (conf->connect_cnt == 0) {
	destroy_sound(conf);
    }

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

    pj_mutex_unlock(conf->mutex);


    /* Stop sound if there's no connection. */
    if (conf->connect_cnt == 0) {
	destroy_sound(conf);
    }

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
    info->clock_rate = conf_port->clock_rate;
    info->samples_per_frame = conf_port->samples_per_frame;

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

/* Copy samples */
PJ_INLINE(void) copy_samples(pj_int16_t *dst, 
				const pj_int16_t *src,
				unsigned count)
{
    unsigned i;
    for (i=0; i<count; ++i)
	dst[i] = src[i];
}

/* Zero samples. */
PJ_INLINE(void) zero_samples(pj_int16_t *buf, unsigned count)
{
    unsigned i;
    for (i=0; i<count; ++i)
	buf[i] = 0;
}


/*
 * Read from port.
 */
static pj_status_t read_port( pjmedia_conf *conf,
			      struct conf_port *cport, pj_int16_t *frame,
			      pj_size_t count, pjmedia_frame_type *type )
{

    pj_assert(count == conf->samples_per_frame);

    /* If port's samples per frame and sampling rate matches conference
     * bridge's settings, get the frame directly from the port.
     */
    if (cport->rx_buf_cap == 0) {
	pjmedia_frame f;
	pj_status_t status;

	f.buf = frame;
	f.size = count * BYTES_PER_SAMPLE;

	status = (cport->port->get_frame)(cport->port, &f);

	*type = f.type;

	return status;

    } else {

	/*
	 * If we don't have enough samples in rx_buf, read from the port 
	 * first. Remember that rx_buf may be in different clock rate!
	 */
	while (cport->rx_buf_count < count * 1.0 *
		cport->clock_rate / conf->clock_rate) {

	    pjmedia_frame f;
	    pj_status_t status;

	    f.buf = cport->rx_buf + cport->rx_buf_count;
	    f.size = cport->samples_per_frame * BYTES_PER_SAMPLE;

	    status = pjmedia_port_get_frame(cport->port, &f);

	    if (status != PJ_SUCCESS) {
		/* Fatal error! */
		return status;
	    }

	    if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
		zero_samples( cport->rx_buf + cport->rx_buf_count,
			      cport->samples_per_frame);
	    }

	    cport->rx_buf_count += cport->samples_per_frame;

	    pj_assert(cport->rx_buf_count <= cport->rx_buf_cap);
	}

	/*
	 * If port's clock_rate is different, resample.
	 * Otherwise just copy.
	 */
	if (cport->clock_rate != conf->clock_rate) {
	    
	    unsigned src_count;

	    pjmedia_resample_run( cport->rx_resample,cport->rx_buf, frame);

	    src_count = (unsigned)(count * 1.0 * cport->clock_rate / 
				   conf->clock_rate);
	    cport->rx_buf_count -= src_count;
	    if (cport->rx_buf_count) {
		copy_samples(cport->rx_buf, cport->rx_buf+src_count,
			     cport->rx_buf_count);
	    }

	} else {

	    copy_samples(frame, cport->rx_buf, count);
	    cport->rx_buf_count -= count;
	    if (cport->rx_buf_count) {
		copy_samples(cport->rx_buf, cport->rx_buf+count,
			     cport->rx_buf_count);
	    }
	}
    }

    return PJ_SUCCESS;
}


/*
 * Write the mixed signal to the port.
 */
static pj_status_t write_port(pjmedia_conf *conf, struct conf_port *cport,
			      pj_uint32_t timestamp)
{
    pj_int16_t *buf;
    unsigned j;

    /* If port is muted or nobody is transmitting to this port, 
     * transmit NULL frame. 
     */
    if (cport->tx_setting == PJMEDIA_PORT_MUTE || cport->sources==0) {

	pjmedia_frame frame;

	frame.type = PJMEDIA_FRAME_TYPE_NONE;
	frame.buf = NULL;
	frame.size = 0;

	if (cport->port)
	    pjmedia_port_put_frame(cport->port, &frame);

	return PJ_SUCCESS;

    } else if (cport->tx_setting != PJMEDIA_PORT_ENABLE) {
	return PJ_SUCCESS;
    }

    /* If there are sources in the mix buffer, convert the mixed samples
     * to the mixed samples itself. This is possible because mixed sample
     * is 32bit.
     */
    buf = (pj_int16_t*)cport->mix_buf;
    for (j=0; j<conf->samples_per_frame; ++j) {
	buf[j] = unsigned2pcm(cport->mix_buf[j] / cport->sources);
    }

    /* If port has the same clock_date and samples_per_frame settings as
     * the conference bridge, transmit the frame as is.
     */
    if (cport->clock_rate == conf->clock_rate &&
	cport->samples_per_frame == conf->samples_per_frame)
    {
	pjmedia_frame frame;

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.buf = (pj_int16_t*)cport->mix_buf;
	frame.size = conf->samples_per_frame * BYTES_PER_SAMPLE;
	frame.timestamp.u64 = timestamp;

	if (cport->port != NULL)
	    return pjmedia_port_put_frame(cport->port, &frame);
	else
	    return PJ_SUCCESS;
    }

    /* If it has different clock_rate, must resample. */
    if (cport->clock_rate != conf->clock_rate) {

	unsigned dst_count;

	pjmedia_resample_run( cport->tx_resample, buf, 
			      cport->tx_buf + cport->tx_buf_count );

	dst_count = (unsigned)(conf->samples_per_frame * 1.0 *
			       cport->clock_rate / conf->clock_rate);
	cport->tx_buf_count += dst_count;

    } else {
	/* Same clock rate.
	 * Just copy the samples to tx_buffer.
	 */
	copy_samples( cport->tx_buf + cport->tx_buf_count,
		      buf, conf->samples_per_frame );
	cport->tx_buf_count += conf->samples_per_frame;
    }

    /* Transmit once we have enough frame in the tx_buf. */
    if (cport->tx_buf_count >= cport->samples_per_frame) {
	
	pjmedia_frame frame;
	pj_status_t status;

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.buf = cport->tx_buf;
	frame.size = cport->samples_per_frame * BYTES_PER_SAMPLE;
	frame.timestamp.u64 = timestamp;

	if (cport->port)
	    status = pjmedia_port_put_frame(cport->port, &frame);
	else
	    status = PJ_SUCCESS;

	cport->tx_buf_count -= cport->samples_per_frame;
	if (cport->tx_buf_count) {
	    copy_samples(cport->tx_buf, 
			 cport->tx_buf + cport->samples_per_frame,
			 cport->tx_buf_count);
	}

	return status;
    }

    return PJ_SUCCESS;
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
    unsigned ci, cj, i, j;
    
    PJ_UNUSED_ARG(timestamp);
    PJ_UNUSED_ARG(size);

    pj_mutex_lock(conf->mutex);

    TRACE_(("p"));

    /* Zero all port's temporary buffers. */
    for (i=0, ci=0; i<conf->max_ports && ci < conf->port_cnt; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pj_uint32_t *mix_buf;

	/* Skip empty slot. */
	if (!conf_port)
	    continue;

	++ci;

	conf_port->sources = 0;
	mix_buf = conf_port->mix_buf;

	for (j=0; j<conf->samples_per_frame; ++j)
	    mix_buf[j] = 0;
    }

    /* Get frames from all ports, and "mix" the signal 
     * to mix_buf of all listeners of the port.
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
	    pj_int16_t *snd_buf;

	    if (conf_port->snd_read_pos == conf_port->snd_write_pos) {
		conf_port->snd_read_pos = 
		    (conf_port->snd_write_pos+RX_BUF_COUNT-RX_BUF_COUNT/2) % 
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

	    snd_buf = conf_port->snd_buf[conf_port->snd_read_pos];
	    for (j=0; j<conf->samples_per_frame; ++j) {
		((pj_int16_t*)output)[j] = snd_buf[j];
	    }
	    conf_port->snd_read_pos = (conf_port->snd_read_pos+1) % RX_BUF_COUNT;

	} else {

	    pj_status_t status;
	    pjmedia_frame_type frame_type;

	    status = read_port(conf, conf_port, output, 
			       conf->samples_per_frame, &frame_type);
	    
	    if (status != PJ_SUCCESS) {
		PJ_LOG(4,(THIS_FILE, "Port %.*s get_frame() returned %d. "
				     "Port is now disabled",
				     (int)conf_port->name.slen,
				     conf_port->name.ptr,
				     status));
		conf_port->rx_setting = PJMEDIA_PORT_DISABLE;
		continue;
	    }
	}

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
	    pj_uint32_t *mix_buf;
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
	    mix_buf = listener->mix_buf;
	    for (k=0; k<conf->samples_per_frame; ++k)
		mix_buf[k] += (conf->uns_buf[k] * level);

	    listener->sources += level;
	}
    }

    /* For all ports, calculate avg signal. */
    for (i=0, ci=0; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *conf_port = conf->ports[i];
	pj_status_t status;

	if (!conf_port)
	    continue;

	++ci;

	status = write_port( conf, conf_port, timestamp);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(4,(THIS_FILE, "Port %.*s put_frame() returned %d. "
				 "Port is now disabled",
				 (int)conf_port->name.slen,
				 conf_port->name.ptr,
				 status));
	    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
	    continue;
	}
    }

    /* Return sound playback frame. */
    if (conf->ports[0]->sources) {
	copy_samples( output, (pj_int16_t*)conf->ports[0]->mix_buf, 
		      conf->samples_per_frame);
    } else {
	zero_samples( output, conf->samples_per_frame ); 
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
    pj_int16_t *target_snd_buf;
    unsigned i;

    PJ_UNUSED_ARG(timestamp);
    
    TRACE_(("r"));

    if (size != conf->samples_per_frame*2) {
	TRACE_(("rxerr "));
    }

    /* Skip if this port is muted/disabled. */
    if (snd_port->rx_setting != PJMEDIA_PORT_ENABLE) {
	return PJ_SUCCESS;
    }

    /* Skip if no port is listening to the microphone */
    if (snd_port->listener_cnt == 0) {
	return PJ_SUCCESS;
    }


    /* Determine which rx_buffer to fill in */
    target_snd_buf = snd_port->snd_buf[snd_port->snd_write_pos];
    
    /* Copy samples from audio device to target rx_buffer */
    for (i=0; i<conf->samples_per_frame; ++i) {
	target_snd_buf[i] = ((pj_int16_t*)input)[i];
    }

    /* Switch buffer */
    snd_port->snd_write_pos = (snd_port->snd_write_pos+1)%RX_BUF_COUNT;


    return PJ_SUCCESS;
}


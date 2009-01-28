/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/conference.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/sound_port.h>
#include <pjmedia/stream.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJMEDIA_CONF_USE_SWITCH_BOARD) && PJMEDIA_CONF_USE_SWITCH_BOARD!=0

/* CONF_DEBUG enables detailed operation of the conference bridge.
 * Beware that it prints large amounts of logs (several lines per frame).
 */
//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif


/* REC_FILE macro enables recording of the samples written to the sound
 * device. The file contains RAW PCM data with no header, and has the
 * same settings (clock rate etc) as the conference bridge.
 * This should only be enabled when debugging audio quality *only*.
 */
//#define REC_FILE    "confrec.pcm"
#ifdef REC_FILE
static FILE *fhnd_rec;
#endif


#define THIS_FILE	    "conf_switch.c"

#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('S', 'W', 'T', 'C')
#define SIGNATURE_PORT	    PJMEDIA_PORT_SIGNATURE('S', 'W', 'T', 'P')
#define NORMAL_LEVEL	    128
#define SLOT_TYPE	    unsigned
#define INVALID_SLOT	    ((SLOT_TYPE)-1)
#define BUFFER_SIZE	    PJMEDIA_MAX_MTU

/*
 * DON'T GET CONFUSED WITH TX/RX!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. So TX means the bridge
 * is transmitting to the port, RX means the bridge is receiving from the
 * port.
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
    unsigned		 listener_cnt;	/**< Number of listeners.	    */
    SLOT_TYPE		*listener_slots;/**< Array of listeners.	    */
    unsigned		 transmitter_cnt;/**<Number of transmitters.	    */

    /* Shortcut for port info. */
    unsigned		 clock_rate;	/**< Port's clock rate.		    */
    unsigned		 samples_per_frame; /**< Port's samples per frame.  */
    unsigned		 channel_count;	/**< Port's channel count.	    */

    /* Calculated signal levels: */
    unsigned		 tx_level;	/**< Last tx level to this port.    */
    unsigned		 rx_level;	/**< Last rx level from this port.  */

    /* The normalized signal level adjustment.
     * A value of 128 (NORMAL_LEVEL) means there's no adjustment.
     */
    unsigned		 tx_adj_level;	/**< Adjustment for TX.		    */
    unsigned		 rx_adj_level;	/**< Adjustment for RX.		    */

    pj_timestamp	 ts_clock;
    pj_timestamp	 ts_rx;

    /* Tx buffer is a temporary buffer to be used when there's mismatch 
     * between port's ptime with conference's ptime. This buffer is used as 
     * the source to buffer the samples until there are enough samples to 
     * fulfill a complete frame to be transmitted to the port.
     */
    pj_uint8_t		 tx_buf[BUFFER_SIZE]; /**< Tx buffer.		    */

    /* When the port is not receiving signal from any other ports (e.g. when
     * no other ports is transmitting to this port), the bridge periodically
     * transmit NULL frame to the port to keep the port "alive" (for example,
     * a stream port needs this heart-beat to periodically transmit silence
     * frame to keep NAT binding alive).
     *
     * This NULL frame should be sent to the port at the port's ptime rate.
     * So if the port's ptime is greater than the bridge's ptime, the bridge
     * needs to delay the NULL frame until it's the right time to do so.
     *
     * This variable keeps track of how many pending NULL samples are being
     * "held" for this port. Once this value reaches samples_per_frame
     * value of the port, a NULL frame is sent. The samples value on this
     * variable is clocked at the port's clock rate.
     */
    unsigned		 tx_heart_beat;
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
    pjmedia_snd_port	 *snd_dev_port;	/**< Sound device port.		    */
    pjmedia_port	 *master_port;	/**< Port zero's port.		    */
    char		  master_name_buf[80]; /**< Port0 name buffer.	    */
    pj_mutex_t		 *mutex;	/**< Conference mutex.		    */
    struct conf_port	**ports;	/**< Array of ports.		    */
    unsigned		  clock_rate;	/**< Sampling rate.		    */
    unsigned		  channel_count;/**< Number of channels (1=mono).   */
    unsigned		  samples_per_frame;	/**< Samples per frame.	    */
    unsigned		  bits_per_sample;	/**< Bits per sample.	    */
    pj_uint8_t		  buf[BUFFER_SIZE];	/**< Common buffer.	    */
};


/* Prototypes */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t destroy_port(pjmedia_port *this_port);


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
    pjmedia_frame *f;

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);

    /* Set name */
    pj_strdup_with_null(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Create transmit flag array */
    conf_port->listener_slots = (SLOT_TYPE*)
				pj_pool_zalloc(pool, 
					  conf->max_ports * sizeof(SLOT_TYPE));
    PJ_ASSERT_RETURN(conf_port->listener_slots, PJ_ENOMEM);

    /* Save some port's infos, for convenience. */
    if (port) {
	conf_port->port = port;
	conf_port->clock_rate = port->info.clock_rate;
	conf_port->samples_per_frame = port->info.samples_per_frame;
	conf_port->channel_count = port->info.channel_count;
    } else {
	conf_port->port = NULL;
	conf_port->clock_rate = conf->clock_rate;
	conf_port->samples_per_frame = conf->samples_per_frame;
	conf_port->channel_count = conf->channel_count;
    }

    /* Init pjmedia_frame structure in the TX buffer. */
    f = (pjmedia_frame*)conf_port->tx_buf;
    f->buf = conf_port->tx_buf + sizeof(pjmedia_frame);
    f->size = 0;

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
    pj_str_t name = { "Master/sound", 12 };
    pj_status_t status;


    status = create_conf_port(pool, conf, NULL, &name, &conf_port);
    if (status != PJ_SUCCESS)
	return status;


    /* Create sound device port: */

    if ((conf->options & PJMEDIA_CONF_NO_DEVICE) == 0) {
	pjmedia_snd_stream *strm;
	pjmedia_snd_stream_info si;

	/*
	 * If capture is disabled then create player only port.
	 * Otherwise create bidirectional sound device port.
	 */
	if (conf->options & PJMEDIA_CONF_NO_MIC)  {
	    status = pjmedia_snd_port_create_player(pool, -1, conf->clock_rate,
						    conf->channel_count,
						    conf->samples_per_frame,
						    conf->bits_per_sample, 
						    0,	/* options */
						    &conf->snd_dev_port);

	} else {
	    status = pjmedia_snd_port_create( pool, -1, -1, conf->clock_rate, 
					      conf->channel_count, 
					      conf->samples_per_frame,
					      conf->bits_per_sample,
					      0,    /* Options */
					      &conf->snd_dev_port);

	}

	if (status != PJ_SUCCESS)
	    return status;

	strm = pjmedia_snd_port_get_snd_stream(conf->snd_dev_port);
	status = pjmedia_snd_stream_get_info(strm, &si);
	if (status == PJ_SUCCESS) {
	    const pjmedia_snd_dev_info *snd_dev_info;
	    if (conf->options & PJMEDIA_CONF_NO_MIC)
		snd_dev_info = pjmedia_snd_get_dev_info(si.play_id);
	    else
		snd_dev_info = pjmedia_snd_get_dev_info(si.rec_id);
	    pj_strdup2_with_null(pool, &conf_port->name, snd_dev_info->name);
	}
    }


     /* Add the port to the bridge */
    conf->ports[0] = conf_port;
    conf->port_cnt++;


    PJ_LOG(5,(THIS_FILE, "Sound device successfully created for port 0"));
    return PJ_SUCCESS;
}

/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					 unsigned max_ports,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 unsigned options,
					 pjmedia_conf **p_conf )
{
    pjmedia_conf *conf;
    const pj_str_t name = { "Conf", 4 };
    pj_status_t status;

    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
	      max_ports));

    /* Create and init conf structure. */
    conf = PJ_POOL_ZALLOC_T(pool, pjmedia_conf);
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);

    conf->ports = (struct conf_port**) 
		  pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->options = options;
    conf->max_ports = max_ports;
    conf->clock_rate = clock_rate;
    conf->channel_count = channel_count;
    conf->samples_per_frame = samples_per_frame;
    conf->bits_per_sample = bits_per_sample;

    
    /* Create and initialize the master port interface. */
    conf->master_port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(conf->master_port, PJ_ENOMEM);
    
    pjmedia_port_info_init(&conf->master_port->info, &name, SIGNATURE,
			   clock_rate, channel_count, bits_per_sample,
			   samples_per_frame);

    conf->master_port->port_data.pdata = conf;
    conf->master_port->port_data.ldata = 0;

    conf->master_port->get_frame = &get_frame;
    conf->master_port->put_frame = &put_frame;
    conf->master_port->on_destroy = &destroy_port;


    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS)
	return status;

    /* Create mutex. */
    status = pj_mutex_create_recursive(pool, "conf", &conf->mutex);
    if (status != PJ_SUCCESS)
	return status;

    /* If sound device was created, connect sound device to the
     * master port.
     */
    if (conf->snd_dev_port) {
	status = pjmedia_snd_port_connect( conf->snd_dev_port, 
					   conf->master_port );
	if (status != PJ_SUCCESS) {
	    pjmedia_conf_destroy(conf);
	    return status;
	}
    }

    /* Done */

    *p_conf = conf;

    return PJ_SUCCESS;
}


/*
 * Pause sound device.
 */
static pj_status_t pause_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}

/*
 * Resume sound device.
 */
static pj_status_t resume_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    /* Destroy sound device port. */
    if (conf->snd_dev_port) {
	pjmedia_snd_port_destroy(conf->snd_dev_port);
	conf->snd_dev_port = NULL;
    }

    /* Destroy mutex */
    pj_mutex_destroy(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Destroy the master port (will destroy the conference)
 */
static pj_status_t destroy_port(pjmedia_port *this_port)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    return pjmedia_conf_destroy(conf);
}

/*
 * Get port zero interface.
 */
PJ_DEF(pjmedia_port*) pjmedia_conf_get_master_port(pjmedia_conf *conf)
{
    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL, NULL);

    /* Can only return port interface when PJMEDIA_CONF_NO_DEVICE was
     * present in the option.
     */
    PJ_ASSERT_RETURN((conf->options & PJMEDIA_CONF_NO_DEVICE) != 0, NULL);
    
    return conf->master_port;
}


/*
 * Set master port name.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_port0_name(pjmedia_conf *conf,
						const pj_str_t *name)
{
    unsigned len;

    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL && name != NULL, PJ_EINVAL);

    len = name->slen;
    if (len > sizeof(conf->master_name_buf))
	len = sizeof(conf->master_name_buf);
    
    if (len > 0) pj_memcpy(conf->master_name_buf, name->ptr, len);

    conf->ports[0]->name.ptr = conf->master_name_buf;
    conf->ports[0]->name.slen = len;

    if (conf->master_port)
	conf->master_port->info.name = conf->ports[0]->name;

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

    PJ_ASSERT_RETURN(conf && pool && strm_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(conf->clock_rate == strm_port->info.clock_rate, 
		     PJMEDIA_ENCCLOCKRATE);
    PJ_ASSERT_RETURN(conf->channel_count == strm_port->info.channel_count, 
		     PJMEDIA_ENCCHANNEL);
    PJ_ASSERT_RETURN(conf->bits_per_sample == strm_port->info.bits_per_sample,
		     PJMEDIA_ENCBITS);
    PJ_ASSERT_RETURN((conf->samples_per_frame %
		     strm_port->info.samples_per_frame==0) ||
		     (strm_port->info.samples_per_frame %
		     conf->samples_per_frame==0),
		     PJMEDIA_ENCSAMPLESPFRAME);

    /* If port_name is not specified, use the port's name */
    if (!port_name)
	port_name = &strm_port->info.name;

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

    /* Create conf port structure. */
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(conf->mutex);
	return status;
    }

    /* Put the port. */
    conf->ports[index] = conf_port;
    conf->port_cnt++;

    /* Done. */
    if (p_port) {
	*p_port = index;
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Add passive port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_passive_port( pjmedia_conf *conf,
						   pj_pool_t *pool,
						   const pj_str_t *name,
						   unsigned clock_rate,
						   unsigned channel_count,
						   unsigned samples_per_frame,
						   unsigned bits_per_sample,
						   unsigned options,
						   unsigned *p_slot,
						   pjmedia_port **p_port )
{
    PJ_UNUSED_ARG(conf);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(bits_per_sample);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(p_slot);
    PJ_UNUSED_ARG(p_port);

    return PJ_ENOTSUP;
}



/*
 * Change TX and RX settings for the port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
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
					       unsigned sink_slot,
					       int level )
{
    struct conf_port *src_port, *dst_port;
    pj_bool_t start_sound = PJ_FALSE;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Ports must be valid. */
    PJ_ASSERT_RETURN(conf->ports[src_slot] != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(conf->ports[sink_slot] != NULL, PJ_EINVAL);

    /* For now, level MUST be zero. */
    PJ_ASSERT_RETURN(level == 0, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    /* Check if source and sink has compatible format */
    if (src_slot != 0 && sink_slot != 0 && 
	src_port->port->info.format.u32 != dst_port->port->info.format.u32)
    {
	pj_mutex_unlock(conf->mutex);
	return PJMEDIA_ENOTCOMPATIBLE;
    }

    /* Check if sink is listening to other ports */
    if (dst_port->transmitter_cnt > 0) {
	pj_mutex_unlock(conf->mutex);
	return PJ_ETOOMANYCONN;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
	if (src_port->listener_slots[i] == sink_slot)
	    break;
    }

    if (i == src_port->listener_cnt) {
	src_port->listener_slots[src_port->listener_cnt] = sink_slot;
	++conf->connect_cnt;
	++src_port->listener_cnt;
	++dst_port->transmitter_cnt;

	if (conf->connect_cnt == 1)
	    start_sound = 1;

	PJ_LOG(4,(THIS_FILE,"Port %d (%.*s) transmitting to port %d (%.*s)",
		  src_slot,
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  sink_slot,
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
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Ports must be valid. */
    PJ_ASSERT_RETURN(conf->ports[src_slot] != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(conf->ports[sink_slot] != NULL, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
	if (src_port->listener_slots[i] == sink_slot)
	    break;
    }

    if (i != src_port->listener_cnt) {
	pj_assert(src_port->listener_cnt > 0 && 
		  src_port->listener_cnt < conf->max_ports);
	pj_assert(dst_port->transmitter_cnt > 0 && 
		  dst_port->transmitter_cnt < conf->max_ports);
	pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE), 
		       src_port->listener_cnt, i);
	--conf->connect_cnt;
	--src_port->listener_cnt;
	--dst_port->transmitter_cnt;

	PJ_LOG(4,(THIS_FILE,
		  "Port %d (%.*s) stop transmitting to port %d (%.*s)",
		  src_slot,
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  sink_slot,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));
    }

    pj_mutex_unlock(conf->mutex);

    if (conf->connect_cnt == 0) {
	pause_sound(conf);
    }

    return PJ_SUCCESS;
}

/*
 * Get number of ports currently registered to the conference bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_port_count(pjmedia_conf *conf)
{
    return conf->port_cnt;
}

/*
 * Get total number of ports connections currently set up in the bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_connect_count(pjmedia_conf *conf)
{
    return conf->connect_cnt;
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
	unsigned j;
	struct conf_port *src_port;

	src_port = conf->ports[i];

	if (!src_port)
	    continue;

	if (src_port->listener_cnt == 0)
	    continue;

	for (j=0; j<src_port->listener_cnt; ++j) {
	    if (src_port->listener_slots[j] == port) {
		pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
			       src_port->listener_cnt, j);
		pj_assert(conf->connect_cnt > 0);
		--conf->connect_cnt;
		--src_port->listener_cnt;
		break;
	    }
	}
    }

    /* Update transmitter_cnt of ports we're transmitting to */
    while (conf_port->listener_cnt) {
	unsigned dst_slot;
	struct conf_port *dst_port;

	dst_slot = conf_port->listener_slots[conf_port->listener_cnt-1];
	dst_port = conf->ports[dst_slot];
	--dst_port->transmitter_cnt;
	--conf_port->listener_cnt;
	pj_assert(conf->connect_cnt > 0);
	--conf->connect_cnt;
    }

    /* Remove the port. */
    conf->ports[port] = NULL;
    --conf->port_cnt;

    pj_mutex_unlock(conf->mutex);


    /* Stop sound if there's no connection. */
    if (conf->connect_cnt == 0) {
	pause_sound(conf);
    }

    return PJ_SUCCESS;
}


/*
 * Enum ports.
 */
PJ_DEF(pj_status_t) pjmedia_conf_enum_ports( pjmedia_conf *conf,
					     unsigned ports[],
					     unsigned *p_count )
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && p_count && ports, PJ_EINVAL);

    for (i=0; i<conf->max_ports && count<*p_count; ++i) {
	if (!conf->ports[i])
	    continue;

	ports[count++] = i;
    }

    *p_count = count;
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

    pj_bzero(info, sizeof(pjmedia_conf_port_info));

    info->slot = slot;
    info->name = conf_port->name;
    info->tx_setting = conf_port->tx_setting;
    info->rx_setting = conf_port->rx_setting;
    info->listener_cnt = conf_port->listener_cnt;
    info->listener_slots = conf_port->listener_slots;
    info->clock_rate = conf_port->clock_rate;
    info->channel_count = conf_port->channel_count;
    info->samples_per_frame = conf_port->samples_per_frame;
    info->bits_per_sample = conf->bits_per_sample;
    info->format = slot? conf_port->port->info.format : 
			 conf->master_port->info.format;

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


/*
 * Get signal level.
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_signal_level( pjmedia_conf *conf,
						   unsigned slot,
						   unsigned *tx_level,
						   unsigned *rx_level)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[slot] != NULL, PJ_EINVAL);

    conf_port = conf->ports[slot];

    if (tx_level != NULL) {
	*tx_level = conf_port->tx_level;
    }

    if (rx_level != NULL) 
	*rx_level = conf_port->rx_level;

    return PJ_SUCCESS;
}


/*
 * Adjust RX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_rx_level( pjmedia_conf *conf,
						  unsigned slot,
						  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[slot] != NULL, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk: 
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    conf_port = conf->ports[slot];

    /* Set normalized adjustment level. */
    conf_port->rx_adj_level = adj_level + NORMAL_LEVEL;

    return PJ_SUCCESS;
}


/*
 * Adjust TX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_tx_level( pjmedia_conf *conf,
						  unsigned slot,
						  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Port must be valid. */
    PJ_ASSERT_RETURN(conf->ports[slot] != NULL, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127,, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    conf_port = conf->ports[slot];

    return PJ_SUCCESS;
}

/* Deliver frm_src to a conference port (via frm_dst), eventually call 
 * port's put_frame() when samples count in the frm_dst are equal to 
 * port's samples_per_frame.
 */
static pj_status_t deliver_frame(struct conf_port *cport_dst,
			         pjmedia_frame *frm_dst,
			         const pjmedia_frame *frm_src)
{
    PJ_TODO(MAKE_SURE_DEST_FRAME_HAS_ENOUGH_SPACE);

    if (frm_src->type == PJMEDIA_FRAME_TYPE_EXTENDED) {
	pjmedia_frame_ext *f_src = (pjmedia_frame_ext*)frm_src;
	pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frm_dst;
	unsigned i;

	/* Copy frame to listener's TX buffer. */
	for (i = 0; i < f_src->subframe_cnt; ++i) {
	    pjmedia_frame_ext_subframe *sf;
	    
	    sf = pjmedia_frame_ext_get_subframe(f_src, i);
	    pjmedia_frame_ext_append_subframe(f_dst, sf->data, sf->bitlen, 
					      f_src->samples_cnt / 
					      f_src->subframe_cnt);

	    /* Check if it's time to deliver the TX buffer to listener, 
	     * i.e: samples count in TX buffer equal to listener's
	     * samples per frame.
	     */
	    if (f_dst->samples_cnt == cport_dst->samples_per_frame)
	    {
		f_dst->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
		if (cport_dst->port) {
		    pjmedia_port_put_frame(cport_dst->port, (pjmedia_frame*)f_dst);

		    /* Reset TX buffer. */
		    f_dst->subframe_cnt = 0;
		    f_dst->samples_cnt = 0;
		}
	    }
	}

    } else {

	pjmedia_frame *f_dst = (pjmedia_frame*)frm_dst;
	pj_int16_t *f_start, *f_end;

	f_start = (pj_int16_t*)frm_src->buf;
	f_end   = f_start + (frm_src->size >> 1);
	while (f_start < f_end) {
	    unsigned nsamples_to_copy, nsamples_req;

	    nsamples_to_copy = f_end - f_start;
	    nsamples_req = cport_dst->samples_per_frame - (f_dst->size >> 1);
	    if (nsamples_to_copy > nsamples_req)
		nsamples_to_copy = nsamples_req;
	    pjmedia_copy_samples((pj_int16_t*)f_dst->buf + (f_dst->size >> 1), 
				 f_start, 
				 nsamples_to_copy);
	    f_dst->size += nsamples_to_copy << 1;
	    f_start += nsamples_to_copy;

	    /* Check if it's time to deliver the TX buffer to listener, 
	     * i.e: samples count in TX buffer equal to listener's
	     * samples per frame.
	     */
	    if ((f_dst->size >> 1) == cport_dst->samples_per_frame)
	    {
		f_dst->type = PJMEDIA_FRAME_TYPE_AUDIO;
		if (cport_dst->port) {
		    pjmedia_port_put_frame(cport_dst->port, f_dst);
		 
		    /* Reset TX buffer. */
		    f_dst->size = 0;
		}
	    }
	}

    }

    return PJ_SUCCESS;
}

/*
 * Player callback.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    unsigned ci, i;
    
    PJ_TODO(ADJUST_AND_CALC_RX_TX_LEVEL_FOR_PCM_FRAMES);

    TRACE_((THIS_FILE, "- clock -"));

    /* Must lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Call get_frame() from all ports (except port 0) that has 
     * receiver and distribute the frame (put the frame to the destination 
     * port's buffer to accommodate different ptime, and ultimately call 
     * put_frame() of that port) to ports that are receiving from this port.
     */
    for (i=1, ci=1; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *cport = conf->ports[i];

	/* Skip empty port. */
	if (!cport)
	    continue;

	/* Var "ci" is to count how many ports have been visited so far. */
	++ci;

	/* Skip if we're not allowed to receive from this port. */
	if (cport->rx_setting == PJMEDIA_PORT_DISABLE) {
	    cport->rx_level = 0;
	    continue;
	}

	/* Also skip if this port doesn't have listeners. */
	if (cport->listener_cnt == 0) {
	    cport->rx_level = 0;
	    continue;
	}

	pj_add_timestamp32(&cport->ts_clock, conf->samples_per_frame);

	/* This loop will make sure the ptime between port & conf port 
	 * are synchronized.
	 */
	while (pj_cmp_timestamp(&cport->ts_clock, &cport->ts_rx) > 0) {
	    pjmedia_frame *f = (pjmedia_frame*) conf->buf;
	    pj_status_t status;
	    unsigned j;

	    pj_add_timestamp32(&cport->ts_rx, cport->samples_per_frame);
	    
	    f->buf = &conf->buf[sizeof(pjmedia_frame)];
	    f->size = BUFFER_SIZE - sizeof(pjmedia_frame);

	    /* Get frame from port. */
	    status = pjmedia_port_get_frame(cport->port, f);
	    if (status != PJ_SUCCESS)
		continue;

	    if (f->type == PJMEDIA_FRAME_TYPE_NONE) {
		if (cport->port->info.format.u32 == PJMEDIA_FOURCC_L16) {
		    pjmedia_zero_samples((pj_int16_t*)f->buf,
					 cport->samples_per_frame);
		    f->size = cport->samples_per_frame << 1;
		    f->type = PJMEDIA_FRAME_TYPE_AUDIO;
		} else {
		    /* Handle DTX */
		    PJ_TODO(HANDLE_DTX);
		}
	    }

	    /* Put the frame to all listeners. */
	    for (j=0; j < cport->listener_cnt; ++j) 
	    {
		struct conf_port *listener;
		pjmedia_frame *frm_dst;

		listener = conf->ports[cport->listener_slots[j]];

		/* Skip if this listener doesn't want to receive audio */
		if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
		    continue;
    	    
		if (listener->port)
		    frm_dst = frame;
		else
		    frm_dst = (pjmedia_frame*)listener->tx_buf;

		status = deliver_frame(listener, frm_dst, f);
		if (status != PJ_SUCCESS)
		    continue;
	    }
	}
	
	/* Keep alive mechanism. */
	PJ_TODO(SEND_KEEP_ALIVE_WHEN_NEEDED);
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Recorder callback.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *port = conf->ports[this_port->port_data.ldata];
    unsigned j;

    /* Check for correct size. */
    PJ_ASSERT_RETURN( frame->size == conf->samples_per_frame *
				     conf->bits_per_sample / 8,
		      PJMEDIA_ENCSAMPLESPFRAME);

    /* Skip if this port is muted/disabled. */
    if (port->rx_setting != PJMEDIA_PORT_ENABLE) {
	return PJ_SUCCESS;
    }

    /* Skip if no port is listening to the microphone */
    if (port->listener_cnt == 0) {
	return PJ_SUCCESS;
    }

    if (frame->type == PJMEDIA_FRAME_TYPE_NONE) {
	if (this_port->info.format.u32 == PJMEDIA_FOURCC_L16) {
	    pjmedia_frame *f = (pjmedia_frame*)port->tx_buf;

	    pjmedia_zero_samples((pj_int16_t*)f->buf,
				 port->samples_per_frame);
	    f->size = port->samples_per_frame << 1;
	    f->type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frame = f;
	} else {
	    /* Handle DTX */
	    PJ_TODO(HANDLE_DTX);
	}
    }

    /* Put the frame to all listeners. */
    for (j=0; j < port->listener_cnt; ++j) 
    {
	struct conf_port *listener;
	pjmedia_frame *frm_dst;
	pj_status_t status;

	listener = conf->ports[port->listener_slots[j]];

	/* Skip if this listener doesn't want to receive audio */
	if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
	    continue;

	/* Skip loopback for now. */
	if (listener == port)
	    continue;
	    
	frm_dst = (pjmedia_frame*)listener->tx_buf;

	status = deliver_frame(listener, frm_dst, frame);
	if (status != PJ_SUCCESS)
	    continue;
    }

    return PJ_SUCCESS;
}

#endif

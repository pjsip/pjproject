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
#ifndef __PJMEDIA_CONF_H__
#define __PJMEDIA_CONF_H__


/**
 * @file conference.h
 * @brief Conference bridge.
 */
#include <pjmedia/port.h>


PJ_BEGIN_DECL


/**
 * Opaque type for conference bridge.
 */
typedef struct pjmedia_conf pjmedia_conf;

/**
 * Conference port info.
 */
typedef struct pjmedia_conf_port_info
{
    unsigned		slot;		    /**< Slot number.		    */
    pj_str_t		name;		    /**< Port name.		    */
    pjmedia_port_op	tx_setting;	    /**< Transmit settings.	    */
    pjmedia_port_op	rx_setting;	    /**< Receive settings.	    */
    pj_bool_t	       *listener;	    /**< Array of listeners.	    */
    unsigned		clock_rate;	    /**< Clock rate of the port.    */
    unsigned		channel_count;	    /**< Number of channels.	    */
    unsigned		samples_per_frame;  /**< Samples per frame	    */
    unsigned		bits_per_sample;    /**< Bits per sample.	    */
    int			tx_adj_level;	    /**< Tx level adjustment.	    */
    int			rx_adj_level;	    /**< Rx level adjustment.	    */
} pjmedia_conf_port_info;


/**
 * Conference port options. The values here can be combined in bitmask to
 * be specified when the conference bridge is created.
 */
enum pjmedia_conf_option
{
    PJMEDIA_CONF_NO_MIC  = 1,	/**< Disable audio streams from the
				     microphone device.			    */
    PJMEDIA_CONF_NO_DEVICE = 2,	/**< Do not create sound device.	    */
    PJMEDIA_CONF_SMALL_FILTER=4,/**< Use small filter table when resampling */
    PJMEDIA_CONF_USE_LINEAR=8,	/**< Use linear resampling instead of filter
				     based.				    */
};


/**
 * Create conference bridge with the specified parameters. The sampling rate,
 * samples per frame, and bits per sample will be used for the internal
 * operation of the bridge (e.g. when mixing audio frames). However, ports 
 * with different configuration may be connected to the bridge. In this case,
 * the bridge is able to perform sampling rate conversion, and buffering in 
 * case the samples per frame is different.
 *
 * For this version of PJMEDIA, only 16bits per sample is supported.
 *
 * For this version of PJMEDIA, the channel count of the ports MUST match
 * the channel count of the bridge.
 *
 * Under normal operation (i.e. when PJMEDIA_CONF_NO_DEVICE option is NOT
 * specified), the bridge internally create an instance of sound device
 * and connect the sound device to port zero of the bridge. 
 *
 * If PJMEDIA_CONF_NO_DEVICE options is specified, no sound device will
 * be created in the conference bridge. Application MUST acquire the port
 * interface of the bridge by calling #pjmedia_conf_get_master_port(), and
 * connect this port interface to a sound device port by calling
 * #pjmedia_snd_port_connect().
 *
 * The sound device is crucial for the bridge's operation, because it provides
 * the bridge with necessary clock to process the audio frames periodically.
 * Internally, the bridge runs when get_frame() to port zero is called.
 *
 * @param pool		    Pool to use to allocate the bridge and 
 *			    additional buffers for the sound device.
 * @param max_slots	    Maximum number of slots/ports to be created in
 *			    the bridge. Note that the bridge internally uses
 *			    one port for the sound device, so the actual 
 *			    maximum number of ports will be less one than
 *			    this value.
 * @param sampling_rate	    Set the sampling rate of the bridge. This value
 *			    is also used to set the sampling rate of the
 *			    sound device.
 * @param channel_count	    Number of channels in the PCM stream. Normally
 *			    the value will be 1 for mono, but application may
 *			    specify a value of 2 for stereo. Note that all
 *			    ports that will be connected to the bridge MUST 
 *			    have the same number of channels as the bridge.
 * @param samples_per_frame Set the number of samples per frame. This value
 *			    is also used to set the sound device.
 * @param bits_per_sample   Set the number of bits per sample. This value
 *			    is also used to set the sound device. Currently
 *			    only 16bit per sample is supported.
 * @param options	    Bitmask options to be set for the bridge. The
 *			    options are constructed from #pjmedia_conf_option
 *			    enumeration.
 * @param p_conf	    Pointer to receive the conference bridge instance.
 *
 * @return		    PJ_SUCCESS if conference bridge can be created.
 */
PJ_DECL(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					  unsigned max_slots,
					  unsigned sampling_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  unsigned options,
					  pjmedia_conf **p_conf );


/**
 * Destroy conference bridge.
 *
 * @param conf		    The conference bridge.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf );


/**
 * Get the master port interface of the conference bridge. The master port
 * corresponds to the port zero of the bridge. This is only usefull when 
 * application wants to manage the sound device by itself, instead of 
 * allowing the bridge to automatically create a sound device implicitly.
 *
 * This function will only return a port interface if PJMEDIA_CONF_NO_DEVICE
 * option was specified when the bridge was created.
 *
 * Application can connect the port returned by this function to a 
 * sound device by calling #pjmedia_snd_port_connect().
 *
 * @param conf		    The conference bridge.
 *
 * @return		    The port interface of port zero of the bridge,
 *			    only when PJMEDIA_CONF_NO_DEVICE options was
 *			    specified when the bridge was created.
 */
PJ_DECL(pjmedia_port*) pjmedia_conf_get_master_port(pjmedia_conf *conf);


/**
 * Add stream port to the conference bridge. By default, the new conference
 * port will have both TX and RX enabled, but it is not connected to any
 * other ports.
 *
 * Application SHOULD call #pjmedia_conf_connect_port() to enable audio
 * transmission and receipt to/from this port.
 *
 * @param conf		The conference bridge.
 * @param pool		Pool to allocate buffers for this port.
 * @param strm_port	Stream port interface.
 * @param name		Optional name for the port. If this value is NULL,
 *			the name will be taken from the name in the port 
 *			info.
 * @param p_slot	Pointer to receive the slot index of the port in
 *			the conference bridge.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
					    pj_pool_t *pool,
					    pjmedia_port *strm_port,
					    const pj_str_t *name,
					    unsigned *p_slot );



/**
 * Change TX and RX settings for the port.
 *
 * @param conf		The conference bridge.
 * @param slot		Port number/slot in the conference bridge.
 * @param tx		Settings for the transmission TO this port.
 * @param rx		Settings for the receipt FROM this port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
						  unsigned slot,
						  pjmedia_port_op tx,
						  pjmedia_port_op rx);


/**
 * Enable unidirectional audio from the specified source slot to the
 * specified sink slot.
 *
 * @param conf		The conference bridge.
 * @param src_slot	Source slot.
 * @param sink_slot	Sink slot.
 * @param level		This argument is reserved for future improvements
 *			where it is possible to adjust the level of signal
 *			transmitted in a specific connection. For now,
 *			this argument MUST be zero.
 *
 * @return		PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
						unsigned src_slot,
						unsigned sink_slot,
						int level );


/**
 * Disconnect unidirectional audio from the specified source to the specified
 * sink slot.
 *
 * @param conf		The conference bridge.
 * @param src_slot	Source slot.
 * @param sink_slot	Sink slot.
 *
 * @reutrn		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_disconnect_port( pjmedia_conf *conf,
						   unsigned src_slot,
						   unsigned sink_slot );


/**
 * Remove the specified port from the conference bridge.
 *
 * @param conf		The conference bridge.
 * @param slot		The port index to be removed.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
					       unsigned slot );



/**
 * Enumerate occupied ports in the bridge.
 *
 * @param conf		The conference bridge.
 * @param ports		Array of port numbers to be filled in.
 * @param count		On input, specifies the maximum number of ports
 *			in the array. On return, it will be filled with
 *			the actual number of ports.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_enum_ports( pjmedia_conf *conf,
					      unsigned ports[],
					      unsigned *count );


/**
 * Get port info.
 *
 * @param conf		The conference bridge.
 * @param slot		Port index.
 * @param info		Pointer to receive the info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_get_port_info( pjmedia_conf *conf,
						 unsigned slot,
						 pjmedia_conf_port_info *info);


/**
 * Get occupied ports info.
 *
 * @param conf		The conference bridge.
 * @param size		On input, contains maximum number of infos
 *			to be retrieved. On output, contains the actual
 *			number of infos that have been copied.
 * @param info		Array of info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_get_ports_info(pjmedia_conf *conf,
						 unsigned *size,
						 pjmedia_conf_port_info info[]
						 );


/**
 * Get last signal level transmitted to or received from the specified port.
 * The signal level is an integer value in zero to 255, with zero indicates
 * no signal, and 255 indicates the loudest signal level.
 *
 * @param conf		The conference bridge.
 * @param slot		Slot number.
 * @param tx_level	Optional argument to receive the level of signal
 *			transmitted to the specified port (i.e. the direction
 *			is from the bridge to the port).
 * @param rx_level	Optional argument to receive the level of signal
 *			received from the port (i.e. the direction is from the
 *			port to the bridge).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_get_signal_level(pjmedia_conf *conf,
						   unsigned slot,
						   unsigned *tx_level,
						   unsigned *rx_level);


/**
 * Adjust the level of signal received from the specified port.
 * Application may adjust the level to make the signal received from the port
 * either louder or more quiet, by giving the value from +127 to -128. The
 * value zero indicates no adjustment, the value -128 will mute the signal, 
 * and the value of +127 will make the signal twice as loud.
 *
 * @param conf		The conference bridge.
 * @param slot		Slot number.
 * @param adj_level	Adjustment level, with valid values are from -128
 *			to +127. A value of zero means there is no level
 *			adjustment to be made, the value -128 will mute the 
 *			signal, and the value of +127 will make the signal 
 *			twice as loud.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_adjust_rx_level( pjmedia_conf *conf,
						   unsigned slot,
						   int adj_level );


/**
 * Adjust the level of signal to be transmitted to the specified port.
 * Application may adjust the level to make the signal transmitted to the port
 * either louder or more quiet, by giving the value from +127 to -128. The
 * value zero indicates no adjustment, the value -128 will mute the signal, 
 * and the value of +127 will make the signal twice as loud.
 *
 * @param conf		The conference bridge.
 * @param slot		Slot number.
 * @param adj_level	Adjustment level, with valid values are from -128
 *			to +127. A value of zero means there is no level
 *			adjustment to be made, the value -128 will mute the 
 *			signal, and the value of +127 will make the signal 
 *			twice as loud.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_adjust_tx_level( pjmedia_conf *conf,
						   unsigned slot,
						   int adj_level );



PJ_END_DECL


#endif	/* __PJMEDIA_CONF_H__ */


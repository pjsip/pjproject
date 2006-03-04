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
    unsigned		slot;
    pj_str_t		name;
    pjmedia_port_op	tx_setting;
    pjmedia_port_op	rx_setting;
    pj_bool_t	       *listener;
    unsigned		clock_rate;
    unsigned		samples_per_frame;
} pjmedia_conf_port_info;


/**
 * Create conference bridge.
 */
PJ_DECL(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					  unsigned max_slots,
					  unsigned sampling_rate,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_conf **p_conf );


/**
 * Destroy conference bridge.
 */
PJ_DECL(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf );


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
 * @param name		Port name.
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
 *
 * @return		PJ_SUCCES on success.
 */
PJ_DECL(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
						unsigned src_slot,
						unsigned sink_slot );


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
						 pjmedia_conf_port_info info[]);


PJ_END_DECL


#endif	/* __PJMEDIA_CONF_H__ */


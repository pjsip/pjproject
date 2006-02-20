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
 * @file conf.h
 * @brief Conference bridge.
 */
#include <pjmedia/types.h>

/**
 * Opaque type for conference bridge.
 */
typedef struct pjmedia_conf pjmedia_conf;


/**
 * Create conference bridge.
 */
PJ_DECL(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					  unsigned max_ports,
					  pjmedia_conf **p_conf );


/**
 * Add stream port to the conference bridge.
 */
PJ_DECL(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
					    pj_pool_t *pool,
					    pjmedia_stream_port *strm_port,
					    const pj_str_t *port_name,
					    unsigned *p_port );


/**
 * Mute or unmute port.
 */
PJ_DECL(pj_status_t) pjmedia_conf_set_mute( pjmedia_conf *conf,
					    unsigned port,
					    pj_bool_t mute );


/**
 * Set the specified port to be member of conference bridge.
 */
PJ_DECL(pj_status_t) pjmedia_conf_set_membership( pjmedia_conf *conf,
						  unsigned port,
						  pj_bool_t enabled );


/**
 * Remove the specified port.
 */
PJ_DECL(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
					       unsigned port );



#endif	/* __PJMEDIA_CONF_H__ */


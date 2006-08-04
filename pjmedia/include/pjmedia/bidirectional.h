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
#ifndef __PJMEDIA_BIDIRECTIONAL_H__
#define __PJMEDIA_BIDIRECTIONAL_H__

/**
 * @file bidirectional.h
 * @brief Create bidirectional port from two unidirectional ports.
 */
#include <pjmedia/port.h>


/**
 * @defgroup PJMEDIA_BIDIRECTIONAL_PORT Bidirectional Port
 * @ingroup PJMEDIA_PORT
 * @brief Create bidirectional port from two unidirectional ports.
 * @{
 */


PJ_BEGIN_DECL


/**
 * Create bidirectional port from two unidirectional ports
 *
 * @param pool		Pool to allocate memory.
 * @param get_port	Port where get_frame() will be directed to.
 * @param put_port	Port where put_frame() will be directed to.
 * @param p_port	Pointer to receive the port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_bidirectional_port_create(pj_pool_t *pool,
						       pjmedia_port *get_port,
						       pjmedia_port *put_port,
						       pjmedia_port **p_port );



PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_BIDIRECTIONAL_H__ */


/* $Id$ */
/* 
 * Copyright (C) 2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_SYNC_PORT_H__
#define __PJMEDIA_SYNC_PORT_H__

/**
 * @file sync_port.h
 * @brief Media clock synchronization port.
 */
#include <pjmedia/port.h>

/**
 * @defgroup PJMEDIA_SYNC_PORT Clock Synchronization Port
 * @ingroup PJMEDIA_PORT_CLOCK
 * @brief Media clock synchronizer
 * @{
 *
 * It is a common problem that audio device does not always provide a smooth
 * clock for application, therefore the application may suffer from jitter, 
 * burst, or even clock skew/drift. Processing audio frames in such 'messy'
 * clock will just increase the complexity. So, before delivering the stream
 * to other components, it could be very helpful to tidy up the clock first.
 *
 * This clock synchronizer port provides solutions in encountering burst and 
 * clock skew, by simply inserting it between the port with 'unhealty' clock
 * (upstream port) and the port requiring 'healthy' clock (downstream port) 
 * using pjmedia port framework. Moreover, it also has an optional feature 
 * to remove jitter by employing an external clock (@ref PJMEDIA_CLOCK),
 * this feature will be enabled when PJMEDIA_SYNC_USE_EXT_CLOCK is specified
 * in @pjmedia_sync_option upon its creation.
 *
 * A synchronizer port internally has instance(s) of @ref PJMED_DELAYBUF, 
 * which provides buffering and wave-form manipulation (shrink/expand) to 
 * handle burst and clock skew/drift, and may also have instance of @ref 
 * PJMEDIA_CLOCK, which provides the external clock timer.
 */

PJ_BEGIN_DECL

/**
 * Synchronization port option.
 */
typedef enum pjmedia_sync_option
{
    /** 
     * When this flag is specified, synchronizer will provide external clock,
     * instead of using original parent clock.
     */
    PJMEDIA_SYNC_USE_EXT_CLOCK = 1,

    /** 
     * When this flag is specified, synchronizer will not destroy downstream
     * port when synchronizer port is destroyed.
     */
    PJMEDIA_SYNC_DONT_DESTROY_DN = 128,

} pjmedia_sync_option;

typedef struct pjmedia_sync_param
{
    pjmedia_sync_option	    options;

} pjmedia_sync_param;

/**
 * Create synchronizer port. 
 *
 * @param pool		Pool to allocate memory.
 * @param dn_port	Downstream port.
 * @param param		Synchronizer param, see @pjmedia_sync_param.
 * @param p_port	Pointer to receive the port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_sync_port_create(pj_pool_t *pool,
					      pjmedia_port *dn_port,
					      const pjmedia_sync_param *param,
					      pjmedia_port **p_port );


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_SYNC_PORT_H__ */

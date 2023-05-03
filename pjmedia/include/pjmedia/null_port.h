/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_NULL_PORT_H__
#define __PJMEDIA_NULL_PORT_H__

/**
 * @file null_port.h
 * @brief Null media port.
 */
#include <pjmedia/port.h>



/**
 * @defgroup PJMEDIA_NULL_PORT Null Port
 * @ingroup PJMEDIA_PORT
 * @brief The simplest type of media port which does nothing.
 * @{
 */


PJ_BEGIN_DECL


/**
 * Create Null port. 
 *
 * @param pool                  Pool to allocate memory.
 * @param sampling_rate         Sampling rate of the port.
 * @param channel_count         Number of channels.
 * @param samples_per_frame     Number of samples per frame.
 * @param bits_per_sample       Number of bits per sample.
 * @param p_port                Pointer to receive the port instance.
 *
 * @return                      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_null_port_create( pj_pool_t *pool,
                                               unsigned sampling_rate,
                                               unsigned channel_count,
                                               unsigned samples_per_frame,
                                               unsigned bits_per_sample,
                                               pjmedia_port **p_port );



PJ_END_DECL

/**
 * @}
 */


#endif  /* __PJMEDIA_NULL_PORT_H__ */

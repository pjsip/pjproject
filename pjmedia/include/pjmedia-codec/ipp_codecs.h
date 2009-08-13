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
#ifndef __PJMEDIA_CODECS_IPP_H__
#define __PJMEDIA_CODECS_IPP_H__

/**
 * @file pjmedia-codec/ipp_codecs.h
 * @brief IPP codecs wrapper.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_IPP_CODEC IPP Codecs
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of IPP codecs
 * @{
 *
 * This section describes functions to register and register IPP codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 * This codec factory contains various codecs, e.g: G.729, G.723.1, G.726, 
 * G.728, G.722.1, AMR.
 */

PJ_BEGIN_DECL

/**
 * Initialize and register IPP codecs factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ipp_init( pjmedia_endpt *endpt );



/**
 * Unregister IPP codecs factory from pjmedia endpoint and deinitialize
 * the IPP codecs library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ipp_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_IPP_H__ */


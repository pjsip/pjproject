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
#ifndef __PJMEDIA_CODEC_ILBC_H__
#define __PJMEDIA_CODEC_ILBC_H__

/**
 * @file pjmedia-codec/ilbc.h
 * @brief iLBC codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_ILBC iLBC Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of iLBC Codec
 * @{
 *
 * This section describes functions to register and register iLBC codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 */

PJ_BEGIN_DECL


/**
 * Initialize and register iLBC codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 * @param mode	    Default decoder mode to be used. Valid values are
 *		    20 and 30 ms. Note that encoder mode follows the
 *		    setting advertised in the remote's SDP.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ilbc_init( pjmedia_endpt *endpt,
					      int mode );



/**
 * Unregister iLBC codec factory from pjmedia endpoint and deinitialize
 * the iLBC codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ilbc_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_ILBC_H__ */


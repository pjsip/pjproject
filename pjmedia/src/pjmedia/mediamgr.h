/* $Id$
 *
 */
/* 
 * PJMEDIA - Multimedia over IP Stack 
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PJMEDIA_MEDIAMGR_H__
#define __PJMEDIA_MEDIAMGR_H__


/**
 * @file mediamgr.h
 * @brief Media Manager.
 */
/**
 * @defgroup PJMED_MGR Media Manager
 * @ingroup PJMEDIA
 * @{
 *
 * The media manager acts as placeholder for endpoint capabilities. Each 
 * media manager will have a codec manager to manage list of codecs installed
 * in the endpoint and a sound device factory.
 *
 * A reference to media manager instance is required when application wants
 * to create a media session (#pj_media_session_create or 
 * #pj_media_session_create_from_sdp).
 */

#include <pjmedia/sound.h>
#include <pjmedia/codec.h>


PJ_BEGIN_DECL


/** Opague declaration of media manager. */
typedef struct pj_med_mgr_t pj_med_mgr_t;

/**
 * Create an instance of media manager.
 *
 * @param pf		Pool factory.
 * @param conn_addr	Connection address to be used by this media manager.
 *
 * @return A new instance of media manager, or NULL if failed.
 */
PJ_DECL(pj_med_mgr_t*) pj_med_mgr_create (pj_pool_factory *pf);

/**
 * Destroy media manager instance.
 *
 * @param mgr		Media manager instance.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_med_mgr_destroy (pj_med_mgr_t *mgr);

/**
 * Get pool factory of the media manager as specified when the media
 * manager was created.
 *
 * @param mgr		The media manager instance.
 *
 * @return Pool factory instance of the media manager.
 */
PJ_DECL(pj_pool_factory*) pj_med_mgr_get_pool_factory (pj_med_mgr_t *mgr);

/**
 * Get the codec manager instance.
 *
 * @param mgr		The media manager instance.
 *
 * @return The instance of codec manager.
 */
PJ_DECL(pj_codec_mgr*) pj_med_mgr_get_codec_mgr (pj_med_mgr_t *mgr);



PJ_END_DECL


/**
 * @}
 */



#endif	/* __PJMEDIA_MEDIAMGR_H__ */

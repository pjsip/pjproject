/* $Id$ */
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
#ifndef __PJMEDIA_AUDIODEV_AUDIODEV_H__
#define __PJMEDIA_AUDIODEV_AUDIODEV_H__

/**
 * @file audiodev.h
 * @brief Audio subsystem API.
 */
#include <pj/pool.h>
#include <pjmedia/audiodev.h>


PJ_BEGIN_DECL

/**
 * @defgroup s2_audio_device_reference Audio Subsystem API Reference
 * @ingroup audio_subsystem_api
 * @brief API Reference
 * @{
 */


/**
 * Initialize the audio subsystem. This will register all supported audio 
 * device factories to the audio subsystem. This function may be called
 * more than once, but each call to this function must have the
 * corresponding #pjmedia_aud_subsys_shutdown() call.
 *
 * @param pf		The pool factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_subsys_init(pj_pool_factory *pf);


/**
 * Get the pool factory registered to the audio subsystem.
 *
 * @return		The pool factory.
 */
PJ_DECL(pj_pool_factory*) pjmedia_aud_subsys_get_pool_factory(void);


/**
 * Shutdown the audio subsystem. This will destroy all audio device factories
 * registered in the audio subsystem. Note that currently opened audio streams
 * may or may not be closed, depending on the implementation of the audio
 * device factories.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_subsys_shutdown(void);


/**
 * Register a supported audio device factory to the audio subsystem. This
 * function can only be called after calling #pjmedia_aud_subsys_init().
 *
 * @param adf		The audio device factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t)
pjmedia_aud_register_factory(pjmedia_aud_dev_factory_create_func_ptr adf);


/**
 * Unregister an audio device factory from the audio subsystem. This
 * function can only be called after calling #pjmedia_aud_subsys_init().
 * Devices from this factory will be unlisted. If a device from this factory
 * is currently in use, then the behavior is undefined.
 *
 * @param adf		The audio device factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t)
pjmedia_aud_unregister_factory(pjmedia_aud_dev_factory_create_func_ptr adf);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_AUDIODEV_AUDIODEV_H__ */


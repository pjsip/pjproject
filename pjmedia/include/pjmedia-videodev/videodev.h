/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_VIDEODEV_VIDEODEV_H__
#define __PJMEDIA_VIDEODEV_VIDEODEV_H__

/**
 * @file videodev.h
 * @brief Video device API.
 */
#include <pjmedia/videodev.h>


PJ_BEGIN_DECL

/**
 * @defgroup video_device_reference Video Subsystem API Reference
 * @ingroup video_subsystem_api
 * @brief API Reference
 * @{
 */
 

/**
 * Initialize the video device subsystem. This will register all supported
 * video device factories to the video device subsystem. This function may be
 * called more than once, but each call to this function must have the
 * corresponding #pjmedia_vid_dev_subsys_shutdown() call.
 *
 * @param pf        The pool factory.
 *
 * @return          PJ_SUCCESS on successful operation or the appropriate
 *                  error code.
 */
PJ_DECL(pj_status_t) pjmedia_vid_dev_subsys_init(pj_pool_factory *pf);


/**
 * Get the pool factory registered to the video device subsystem.
 *
 * @return          The pool factory.
 */
PJ_DECL(pj_pool_factory*) pjmedia_vid_dev_subsys_get_pool_factory(void);


/**
 * Shutdown the video device subsystem. This will destroy all video device
 * factories registered in the video device subsystem. Note that currently
 * opened video streams may or may not be closed, depending on the
 * implementation of the video device factories.
 *
 * @return          PJ_SUCCESS on successful operation or the appropriate
 *                  error code.
 */
PJ_DECL(pj_status_t) pjmedia_vid_dev_subsys_shutdown(void);


/**
 * Register a supported video device factory to the video device subsystem.
 * Application can either register a function to create the factory, or
 * an instance of an already created factory.
 *
 * This function can only be called after calling
 * #pjmedia_vid_dev_subsys_init().
 *
 * @param vdf       The factory creation function. Either vdf or factory
 * 		    argument must be specified.
 * @param factory   Factory instance. Either vdf or factory
 * 		    argument must be specified.
 *
 * @return          PJ_SUCCESS on successful operation or the appropriate
 *                  error code.
 */
PJ_DECL(pj_status_t)
pjmedia_vid_register_factory(pjmedia_vid_dev_factory_create_func_ptr vdf,
                             pjmedia_vid_dev_factory *factory);


/**
 * Unregister a video device factory from the video device subsystem. This
 * function can only be called after calling #pjmedia_vid_dev_subsys_init().
 * Devices from this factory will be unlisted. If a device from this factory
 * is currently in use, then the behavior is undefined.
 *
 * @param vdf       The video device factory. Either vdf or factory argument
 * 		    must be specified.
 * @param factory   The factory instance. Either vdf or factory argument
 * 		    must be specified.
 *
 * @return          PJ_SUCCESS on successful operation or the appropriate
 *                  error code.
 */
PJ_DECL(pj_status_t)
pjmedia_vid_unregister_factory(pjmedia_vid_dev_factory_create_func_ptr vdf,
                               pjmedia_vid_dev_factory *factory);


/**
 * @}
 */

PJ_END_DECL


#endif    /* __PJMEDIA_VIDEODEV_VIDEODEV_H__ */

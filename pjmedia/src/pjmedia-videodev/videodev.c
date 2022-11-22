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
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define THIS_FILE   "videodev.c"


/* extern functions to create factories */
#if PJMEDIA_VIDEO_DEV_HAS_DSHOW
pjmedia_vid_dev_factory* pjmedia_dshow_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_CBAR_SRC
pjmedia_vid_dev_factory* pjmedia_cbar_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_SDL
pjmedia_vid_dev_factory* pjmedia_sdl_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_FFMPEG
pjmedia_vid_dev_factory* pjmedia_ffmpeg_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_V4L2
pjmedia_vid_dev_factory* pjmedia_v4l2_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_QT
pjmedia_vid_dev_factory* pjmedia_qt_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_DARWIN
pjmedia_vid_dev_factory* pjmedia_darwin_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_OPENGL
pjmedia_vid_dev_factory* pjmedia_opengl_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_VIDEO_DEV_HAS_ANDROID
pjmedia_vid_dev_factory* pjmedia_and_factory(pj_pool_factory *pf);
#endif

#define MAX_DRIVERS     PJMEDIA_VID_DEV_MAX_DRIVERS
#define MAX_DEVS        PJMEDIA_VID_DEV_MAX_DEVS


/* API: Initialize the video device subsystem. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_subsys_init(pj_pool_factory *pf)
{
    unsigned i;
    pj_status_t status = PJ_SUCCESS;
    pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();

    /* Allow init() to be called multiple times as long as there is matching
     * number of shutdown().
     */
    if (vid_subsys->init_count++ != 0) {
        return PJ_SUCCESS;
    }

    /* Register error subsystem */
    pj_register_strerror(PJMEDIA_VIDEODEV_ERRNO_START, 
                         PJ_ERRNO_SPACE_SIZE, 
                         &pjmedia_videodev_strerror);

    /* Init */
    vid_subsys->pf = pf;
    vid_subsys->drv_cnt = 0;
    vid_subsys->dev_cnt = 0;

    /* Register creation functions */
#if PJMEDIA_VIDEO_DEV_HAS_V4L2
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_v4l2_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_QT
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_qt_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_OPENGL
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_opengl_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_DARWIN
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_darwin_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_DSHOW
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_dshow_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_FFMPEG
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_ffmpeg_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_SDL
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_sdl_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_ANDROID
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_and_factory;
#endif
#if PJMEDIA_VIDEO_DEV_HAS_CBAR_SRC
    /* Better put colorbar at the last, so the default capturer will be
     * a real capturer, if any.
     */
    vid_subsys->drv[vid_subsys->drv_cnt++].create = &pjmedia_cbar_factory;
#endif

    /* Initialize each factory and build the device ID list */
    for (i=0; i<vid_subsys->drv_cnt; ++i) {
        status = pjmedia_vid_driver_init(i, PJ_FALSE);
        if (status != PJ_SUCCESS) {
            pjmedia_vid_driver_deinit(i);
            continue;
        }
    }

    return vid_subsys->dev_cnt ? PJ_SUCCESS : status;
}

/* API: register a video device factory to the video device subsystem. */
PJ_DEF(pj_status_t)
pjmedia_vid_register_factory(pjmedia_vid_dev_factory_create_func_ptr adf,
                             pjmedia_vid_dev_factory *factory)
{
    pj_bool_t refresh = PJ_FALSE;
    pj_status_t status;
    pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();

    if (vid_subsys->init_count == 0)
        return PJMEDIA_EVID_INIT;

    vid_subsys->drv[vid_subsys->drv_cnt].create = adf;
    vid_subsys->drv[vid_subsys->drv_cnt].f = factory;

    if (factory) {
        /* Call factory->init() */
        status = factory->op->init(factory);
        if (status != PJ_SUCCESS) {
            factory->op->destroy(factory);
            return status;
        }
        refresh = PJ_TRUE;
    }

    status = pjmedia_vid_driver_init(vid_subsys->drv_cnt, refresh);
    if (status == PJ_SUCCESS) {
        vid_subsys->drv_cnt++;
    } else {
        pjmedia_vid_driver_deinit(vid_subsys->drv_cnt);
    }

    return status;
}

/* API: unregister a video device factory from the video device subsystem. */
PJ_DEF(pj_status_t)
pjmedia_vid_unregister_factory(pjmedia_vid_dev_factory_create_func_ptr adf,
                               pjmedia_vid_dev_factory *factory)
{
    unsigned i, j;
    pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();

    if (vid_subsys->init_count == 0)
        return PJMEDIA_EVID_INIT;

    for (i=0; i<vid_subsys->drv_cnt; ++i) {
        pjmedia_vid_driver *drv = &vid_subsys->drv[i];

        if ((factory && drv->f==factory) || (adf && drv->create == adf)) {
            for (j = drv->start_idx; j < drv->start_idx + drv->dev_cnt; j++)
            {
                vid_subsys->dev_list[j] = (pj_uint32_t)PJMEDIA_VID_INVALID_DEV;
            }

            pjmedia_vid_driver_deinit(i);
            pj_bzero(drv, sizeof(*drv));
            return PJ_SUCCESS;
        }
    }

    return PJMEDIA_EVID_ERR;
}

/* API: get the pool factory registered to the video device subsystem. */
PJ_DEF(pj_pool_factory*) pjmedia_vid_dev_subsys_get_pool_factory(void)
{
    pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();
    return vid_subsys->pf;
}

/* API: Shutdown the video device subsystem. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_subsys_shutdown(void)
{
    unsigned i;
    pjmedia_vid_subsys *vid_subsys = pjmedia_get_vid_subsys();

    /* Allow shutdown() to be called multiple times as long as there is
     * matching number of init().
     */
    if (vid_subsys->init_count == 0) {
        return PJ_SUCCESS;
    }
    --vid_subsys->init_count;

    if (vid_subsys->init_count == 0) {
        for (i=0; i<vid_subsys->drv_cnt; ++i) {
            pjmedia_vid_driver_deinit(i);
        }

        pj_bzero(vid_subsys, sizeof(pjmedia_vid_subsys));
    }
    return PJ_SUCCESS;
}


#endif /* PJMEDIA_HAS_VIDEO */

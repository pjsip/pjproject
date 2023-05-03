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
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define THIS_FILE   "videodev.c"

#define DEFINE_CAP(name, info)  {name, info}

/* Capability names */
static struct cap_info
{
    const char *name;
    const char *info;
} cap_infos[] = 
{
    DEFINE_CAP("format",        "Video format"),
    DEFINE_CAP("scale",         "Input dimension"),
    DEFINE_CAP("window",        "Window handle"),
    DEFINE_CAP("resize",        "Renderer resize"),
    DEFINE_CAP("position",      "Renderer position"),
    DEFINE_CAP("hide",          "Renderer hide"),
    DEFINE_CAP("preview",       "Input preview"),
    DEFINE_CAP("orientation",   "Video orientation"),
    DEFINE_CAP("switch",        "Switch device"),
    DEFINE_CAP("wndflags",      "Window flags"),
    DEFINE_CAP("fullscreen",    "Renderer fullscreen")
};


/*
 * The device index seen by application and driver is different. 
 *
 * At application level, device index is index to global list of device.
 * At driver level, device index is index to device list on that particular
 * factory only.
 */
#define MAKE_DEV_ID(f_id, index)   (((f_id & 0xFFFF) << 16) | (index & 0xFFFF))
#define GET_INDEX(dev_id)          ((dev_id) & 0xFFFF)
#define GET_FID(dev_id)            ((dev_id) >> 16)
#define DEFAULT_DEV_ID              0


#define MAX_DRIVERS     PJMEDIA_VID_DEV_MAX_DRIVERS
#define MAX_DEVS        PJMEDIA_VID_DEV_MAX_DEVS

static pjmedia_vid_subsys vid_subsys;


/* API: get the video subsystem. */
PJ_DEF(pjmedia_vid_subsys*) pjmedia_get_vid_subsys(void)
{
    return &vid_subsys;
}


/* API: get capability name/info */
PJ_DEF(const char*) pjmedia_vid_dev_cap_name(pjmedia_vid_dev_cap cap,
                                             const char **p_desc)
{
    const char *desc;
    unsigned i;

    if (p_desc==NULL) p_desc = &desc;

    for (i=0; i<PJ_ARRAY_SIZE(cap_infos); ++i) {
        if ((1 << i)==(int)cap)
            break;
    }

    if (i==PJ_ARRAY_SIZE(cap_infos)) {
        *p_desc = "??";
        return "??";
    }

    *p_desc = cap_infos[i].info;
    return cap_infos[i].name;
}

static pj_status_t get_cap_pointer(const pjmedia_vid_dev_param *param,
                                   pjmedia_vid_dev_cap cap,
                                   void **ptr,
                                   unsigned *size)
{
#define FIELD_INFO(name)    *ptr = (void*)&param->name; \
                            *size = sizeof(param->name)

    switch (cap) {
    case PJMEDIA_VID_DEV_CAP_FORMAT:
        FIELD_INFO(fmt);
        break;
    case PJMEDIA_VID_DEV_CAP_INPUT_SCALE:
        FIELD_INFO(disp_size);
        break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
        FIELD_INFO(window);
        break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE:
        FIELD_INFO(disp_size);
        break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION:
        FIELD_INFO(window_pos);
        break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE:
        FIELD_INFO(window_hide);
        break;
    case PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW:
        FIELD_INFO(native_preview);
        break;
    case PJMEDIA_VID_DEV_CAP_ORIENTATION:
        FIELD_INFO(orient);
        break;
    /* The PJMEDIA_VID_DEV_CAP_SWITCH does not have an entry in the
     * param (it doesn't make sense to open a stream and tell it
     * to switch immediately).
     */
    case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS:
        FIELD_INFO(window_flags);
        break;
    case PJMEDIA_VID_DEV_CAP_OUTPUT_FULLSCREEN:
        FIELD_INFO(window_fullscreen);
        break;
    default:
        return PJMEDIA_EVID_INVCAP;
    }

#undef FIELD_INFO

    return PJ_SUCCESS;
}

/* API: set cap value to param */
PJ_DEF(pj_status_t)
pjmedia_vid_dev_param_set_cap( pjmedia_vid_dev_param *param,
                               pjmedia_vid_dev_cap cap,
                               const void *pval)
{
    void *cap_ptr;
    unsigned cap_size;
    pj_status_t status;

    status = get_cap_pointer(param, cap, &cap_ptr, &cap_size);
    if (status != PJ_SUCCESS)
        return status;

    pj_memcpy(cap_ptr, pval, cap_size);
    param->flags |= cap;

    return PJ_SUCCESS;
}

/* API: get cap value from param */
PJ_DEF(pj_status_t)
pjmedia_vid_dev_param_get_cap( const pjmedia_vid_dev_param *param,
                               pjmedia_vid_dev_cap cap,
                               void *pval)
{
    void *cap_ptr;
    unsigned cap_size;
    pj_status_t status;

    status = get_cap_pointer(param, cap, &cap_ptr, &cap_size);
    if (status != PJ_SUCCESS)
        return status;

    if ((param->flags & cap) == 0) {
        pj_bzero(cap_ptr, cap_size);
        return PJMEDIA_EVID_INVCAP;
    }

    pj_memcpy(pval, cap_ptr, cap_size);
    return PJ_SUCCESS;
}

/* API: init driver */
PJ_DEF(pj_status_t) pjmedia_vid_driver_init(unsigned drv_idx,
                                            pj_bool_t refresh)
{
    pjmedia_vid_driver *drv = &vid_subsys.drv[drv_idx];
    pjmedia_vid_dev_factory *f;
    unsigned i, dev_cnt;
    pj_status_t status;

    if (!refresh) {
        /* Create the factory */
        f = (*drv->create)(vid_subsys.pf);
        if (!f)
            return PJ_EUNKNOWN;

        /* Call factory->init() */
        status = f->op->init(f);
        if (status != PJ_SUCCESS) {
            f->op->destroy(f);
            return status;
        }
    } else {
        f = drv->f;
        if (!f)
            return PJ_EINVALIDOP;
    }

    /* Get number of devices */
    dev_cnt = f->op->get_dev_count(f);
    if (dev_cnt + vid_subsys.dev_cnt > MAX_DEVS) {
        PJ_LOG(4,(THIS_FILE, "%d device(s) cannot be registered because"
                              " there are too many devices",
                              vid_subsys.dev_cnt + dev_cnt - MAX_DEVS));
        dev_cnt = MAX_DEVS - vid_subsys.dev_cnt;
    }

    /* enabling this will cause pjsua-lib initialization to fail when there
     * is no video device installed in the system, even when pjsua has been
     * run with --null-video
     *
    if (dev_cnt == 0) {
        f->op->destroy(f);
        return PJMEDIA_EVID_NODEV;
    }
    */

    /* Fill in default devices */
    drv->rend_dev_idx = drv->cap_dev_idx = -1;
    for (i=0; i<dev_cnt; ++i) {
        pjmedia_vid_dev_info info;

        status = f->op->get_dev_info(f, i, &info);
        if (status != PJ_SUCCESS) {
            f->op->destroy(f);
            return status;
        }

        if (drv->name[0]=='\0') {
            /* Set driver name */
            pj_ansi_strxcpy(drv->name, info.driver, sizeof(drv->name));
        }

        if (drv->rend_dev_idx < 0 && (info.dir & PJMEDIA_DIR_RENDER)) {
            /* Set default render device */
            drv->rend_dev_idx = i;
        }
        if (drv->cap_dev_idx < 0 && (info.dir & PJMEDIA_DIR_CAPTURE)) {
            /* Set default capture device */
            drv->cap_dev_idx = i;
        }

        if (drv->rend_dev_idx >= 0 && drv->cap_dev_idx >= 0) {
            /* Done. */
            break;
        }
    }

    /* Register the factory */
    drv->f = f;
    drv->f->sys.drv_idx = drv_idx;
    drv->start_idx = vid_subsys.dev_cnt;
    drv->dev_cnt = dev_cnt;

    /* Register devices to global list */
    for (i=0; i<dev_cnt; ++i) {
        vid_subsys.dev_list[vid_subsys.dev_cnt++] = MAKE_DEV_ID(drv_idx, i);
    }

    return PJ_SUCCESS;
}

/* API: deinit driver */
PJ_DEF(void) pjmedia_vid_driver_deinit(unsigned drv_idx)
{
    pjmedia_vid_driver *drv = &vid_subsys.drv[drv_idx];

    if (drv->f) {
        drv->f->op->destroy(drv->f);
        drv->f = NULL;
    }

    drv->dev_cnt = 0;
    drv->rend_dev_idx = drv->cap_dev_idx = -1;
}


/* API: Refresh the list of video devices installed in the system. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_refresh(void)
{
    unsigned i;
    
    vid_subsys.dev_cnt = 0;
    for (i=0; i<vid_subsys.drv_cnt; ++i) {
        pjmedia_vid_driver *drv = &vid_subsys.drv[i];
        
        if (drv->f && drv->f->op->refresh) {
            pj_status_t status = drv->f->op->refresh(drv->f);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4, (THIS_FILE, status, "Unable to refresh device "
                                                 "list for %s", drv->name));
            }
        }
        pjmedia_vid_driver_init(i, PJ_TRUE);
    }
    return PJ_SUCCESS;
}

/* API: Get the number of video devices installed in the system. */
PJ_DEF(unsigned) pjmedia_vid_dev_count(void)
{
    return vid_subsys.dev_cnt;
}

/* Internal: convert local index to global device index */
static pj_status_t make_global_index(unsigned drv_idx, 
                                     pjmedia_vid_dev_index *id)
{
    if (*id < 0) {
        return PJ_SUCCESS;
    }

    /* Check that factory still exists */
    PJ_ASSERT_RETURN(vid_subsys.drv[drv_idx].f, PJ_EBUG);

    /* Check that device index is valid */
    PJ_ASSERT_RETURN(*id>=0 && *id<(int)vid_subsys.drv[drv_idx].dev_cnt, 
                     PJ_EBUG);

    *id += vid_subsys.drv[drv_idx].start_idx;
    return PJ_SUCCESS;
}

/* Internal: lookup device id */
static pj_status_t lookup_dev(pjmedia_vid_dev_index id,
                              pjmedia_vid_dev_factory **p_f,
                              unsigned *p_local_index)
{
    int f_id, index;

    if (id < 0) {
        unsigned i;

        if (id <= PJMEDIA_VID_INVALID_DEV)
            return PJMEDIA_EVID_INVDEV;

        for (i=0; i<vid_subsys.drv_cnt; ++i) {
            pjmedia_vid_driver *drv = &vid_subsys.drv[i];
            if (id==PJMEDIA_VID_DEFAULT_CAPTURE_DEV && 
                drv->cap_dev_idx >= 0) 
            {
                id = drv->cap_dev_idx;
                make_global_index(i, &id);
                break;
            } else if (id==PJMEDIA_VID_DEFAULT_RENDER_DEV && 
                drv->rend_dev_idx >= 0) 
            {
                id = drv->rend_dev_idx;
                make_global_index(i, &id);
                break;
            }
        }

        if (id < 0) {
            return PJMEDIA_EVID_NODEFDEV;
        }
    }

    f_id = GET_FID(vid_subsys.dev_list[id]);
    index = GET_INDEX(vid_subsys.dev_list[id]);

    if (f_id < 0 || f_id >= (int)vid_subsys.drv_cnt)
        return PJMEDIA_EVID_INVDEV;

    if (index < 0 || index >= (int)vid_subsys.drv[f_id].dev_cnt)
        return PJMEDIA_EVID_INVDEV;

    *p_f = vid_subsys.drv[f_id].f;
    *p_local_index = (unsigned)index;

    return PJ_SUCCESS;

}

/* API: lookup device id */
PJ_DEF(pj_status_t)
pjmedia_vid_dev_get_local_index(pjmedia_vid_dev_index id,
                                pjmedia_vid_dev_factory **p_f,
                                unsigned *p_local_index)
{
    return lookup_dev(id, p_f, p_local_index);
}

/* API: from factory and local index, get global index */
PJ_DEF(pj_status_t)
pjmedia_vid_dev_get_global_index(const pjmedia_vid_dev_factory *f,
                                 unsigned local_idx,
                                 pjmedia_vid_dev_index *pid)
{
    PJ_ASSERT_RETURN(f->sys.drv_idx < MAX_DRIVERS, PJ_EINVALIDOP);
    *pid = local_idx;
    return make_global_index(f->sys.drv_idx, pid);
}

/* API: Get device information. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_get_info(pjmedia_vid_dev_index id,
                                             pjmedia_vid_dev_info *info)
{
    pjmedia_vid_dev_factory *f;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(info, PJ_EINVAL);
    PJ_ASSERT_RETURN(vid_subsys.pf, PJMEDIA_EVID_INIT);

    if (id <= PJMEDIA_VID_INVALID_DEV)
        return PJMEDIA_EVID_INVDEV;

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
        return status;

    status = f->op->get_dev_info(f, index, info);

    /* Make sure device ID is the real ID (not PJMEDIA_VID_DEFAULT_*_DEV) */
    info->id = index;
    make_global_index(f->sys.drv_idx, &info->id);

    return status;
}

/* API: find device */
PJ_DEF(pj_status_t) pjmedia_vid_dev_lookup( const char *drv_name,
                                            const char *dev_name,
                                            pjmedia_vid_dev_index *id)
{
    pjmedia_vid_dev_factory *f = NULL;
    unsigned drv_idx, dev_idx;

    PJ_ASSERT_RETURN(drv_name && dev_name && id, PJ_EINVAL);
    PJ_ASSERT_RETURN(vid_subsys.pf, PJMEDIA_EVID_INIT);

    for (drv_idx=0; drv_idx<vid_subsys.drv_cnt; ++drv_idx) {
        if (!pj_ansi_stricmp(drv_name, vid_subsys.drv[drv_idx].name))
        {
            f = vid_subsys.drv[drv_idx].f;
            break;
        }
    }

    if (!f)
        return PJ_ENOTFOUND;

    for (dev_idx=0; dev_idx<vid_subsys.drv[drv_idx].dev_cnt; ++dev_idx)
    {
        pjmedia_vid_dev_info info;
        pj_status_t status;

        status = f->op->get_dev_info(f, dev_idx, &info);
        if (status != PJ_SUCCESS)
            return status;

        if (!pj_ansi_stricmp(dev_name, info.name))
            break;
    }

    if (dev_idx==vid_subsys.drv[drv_idx].dev_cnt)
        return PJ_ENOTFOUND;

    *id = dev_idx;
    make_global_index(drv_idx, id);

    return PJ_SUCCESS;
}

/* API: Initialize the video device parameters with default values for the
 * specified device.
 */
PJ_DEF(pj_status_t) pjmedia_vid_dev_default_param(pj_pool_t *pool,
                                                  pjmedia_vid_dev_index id,
                                                  pjmedia_vid_dev_param *param)
{
    pjmedia_vid_dev_factory *f;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);
    PJ_ASSERT_RETURN(vid_subsys.pf, PJMEDIA_EVID_INIT);

    if (id <= PJMEDIA_VID_INVALID_DEV)
        return PJMEDIA_EVID_INVDEV;

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
        return status;

    status = f->op->default_param(pool, f, index, param);
    if (status != PJ_SUCCESS)
        return status;

    /* Normalize device IDs */
    make_global_index(f->sys.drv_idx, &param->cap_id);
    make_global_index(f->sys.drv_idx, &param->rend_id);

    return PJ_SUCCESS;
}

/* API: Open video stream object using the specified parameters. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_create(
                                        pjmedia_vid_dev_param *prm,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm)
{
    pjmedia_vid_dev_factory *cap_f=NULL, *rend_f=NULL, *f=NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(prm && prm->dir && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(vid_subsys.pf, PJMEDIA_EVID_INIT);
    PJ_ASSERT_RETURN(prm->dir==PJMEDIA_DIR_CAPTURE ||
                     prm->dir==PJMEDIA_DIR_RENDER ||
                     prm->dir==PJMEDIA_DIR_CAPTURE_RENDER,
                     PJ_EINVAL);

    /* Normalize cap_id */
    if (prm->dir & PJMEDIA_DIR_CAPTURE) {
        unsigned index;

        if (prm->cap_id < 0)
            prm->cap_id = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;

        status = lookup_dev(prm->cap_id, &cap_f, &index);
        if (status != PJ_SUCCESS)
            return status;

        prm->cap_id = index;
        f = cap_f;
    }

    /* Normalize rend_id */
    if (prm->dir & PJMEDIA_DIR_RENDER) {
        unsigned index;

        if (prm->rend_id < 0)
            prm->rend_id = PJMEDIA_VID_DEFAULT_RENDER_DEV;

        status = lookup_dev(prm->rend_id, &rend_f, &index);
        if (status != PJ_SUCCESS)
            return status;

        prm->rend_id = index;
        f = rend_f;
    }

    PJ_ASSERT_RETURN(f != NULL, PJ_EBUG);

    /* For now, cap_id and rend_id must belong to the same factory */
    PJ_ASSERT_RETURN((prm->dir != PJMEDIA_DIR_CAPTURE_RENDER) ||
                     (cap_f == rend_f),
                     PJMEDIA_EVID_INVDEV);

    /* Create the stream */
    status = f->op->create_stream(f, prm, cb,
                                  user_data, p_vid_strm);
    if (status != PJ_SUCCESS)
        return status;

    /* Assign factory id to the stream */
    (*p_vid_strm)->sys.drv_idx = f->sys.drv_idx;
    return PJ_SUCCESS;
}

/* API: Get the running parameters for the specified video stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_get_param(
                                            pjmedia_vid_dev_stream *strm,
                                            pjmedia_vid_dev_param *param)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(strm && param, PJ_EINVAL);
    PJ_ASSERT_RETURN(vid_subsys.pf, PJMEDIA_EVID_INIT);

    status = strm->op->get_param(strm, param);
    if (status != PJ_SUCCESS)
        return status;

    /* Normalize device id's */
    make_global_index(strm->sys.drv_idx, &param->cap_id);
    make_global_index(strm->sys.drv_idx, &param->rend_id);

    return PJ_SUCCESS;
}

/* API: Get the value of a specific capability of the video stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_get_cap(
                                            pjmedia_vid_dev_stream *strm,
                                            pjmedia_vid_dev_cap cap,
                                            void *value)
{
    return strm->op->get_cap(strm, cap, value);
}

/* API: Set the value of a specific capability of the video stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_set_cap(
                                            pjmedia_vid_dev_stream *strm,
                                            pjmedia_vid_dev_cap cap,
                                            const void *value)
{
    /* For fast switching, device global index needs to be converted to
     * local index before forwarding the request to the device stream.
     */
    if (cap == PJMEDIA_VID_DEV_CAP_SWITCH) {
        pjmedia_vid_dev_factory *f;
        unsigned local_idx;
        pj_status_t status;
        pjmedia_vid_dev_switch_param p = *(pjmedia_vid_dev_switch_param*)value;

        status = lookup_dev(p.target_id, &f, &local_idx);
        if (status != PJ_SUCCESS)
            return status;

        /* Make sure that current & target devices share the same factory */
        if (f->sys.drv_idx != strm->sys.drv_idx)
            return PJMEDIA_EVID_INVDEV;

        p.target_id = local_idx;
        return strm->op->set_cap(strm, cap, &p);
    }

    return strm->op->set_cap(strm, cap, value);
}

/* API: Start the stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_start(pjmedia_vid_dev_stream *strm)
{
    pj_status_t status;

    if (pjmedia_vid_dev_stream_is_running(strm))
        return PJ_SUCCESS;

    status = strm->op->start(strm);
    if (status == PJ_SUCCESS)
        strm->sys.is_running = PJ_TRUE;
    return status;
}

/* API: has it been started? */
PJ_DEF(pj_bool_t)
pjmedia_vid_dev_stream_is_running(pjmedia_vid_dev_stream *strm)
{
    return strm->sys.is_running;
}

PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_get_frame(
                                            pjmedia_vid_dev_stream *strm,
                                            pjmedia_frame *frame)
{
    pj_assert(strm->op->get_frame);
    return strm->op->get_frame(strm, frame);
}

PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_put_frame(
                                            pjmedia_vid_dev_stream *strm,
                                            const pjmedia_frame *frame)
{
    pj_assert(strm->op->put_frame);
    return strm->op->put_frame(strm, frame);
}

/* API: Stop the stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_stop(pjmedia_vid_dev_stream *strm)
{
    strm->sys.is_running = PJ_FALSE;
    return strm->op->stop(strm);
}

/* API: Destroy the stream. */
PJ_DEF(pj_status_t) pjmedia_vid_dev_stream_destroy(
                                                pjmedia_vid_dev_stream *strm)
{
    strm->sys.is_running = PJ_FALSE;
    return strm->op->destroy(strm);
}


#endif /* PJMEDIA_HAS_VIDEO */

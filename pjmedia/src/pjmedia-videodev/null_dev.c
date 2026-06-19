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
#include <pj/log.h>
#include <pj/os.h>


#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_NULL) && \
    PJMEDIA_VIDEO_DEV_HAS_NULL != 0


#define THIS_FILE       "null_dev.c"
#define DRIVER_NAME     "Null"
#define DEFAULT_WIDTH   352
#define DEFAULT_HEIGHT  288
#define DEFAULT_FPS     25
#define DEFAULT_CLOCK   90000


/* Supported formats. The two zero-payload planar formats are enough to cover
 * the common pjmedia video stream pipelines (audio-paired video stream uses
 * I420 by default); the renderer doesn't care about content.
 */
static pjmedia_format_id null_fmts[] = {
    PJMEDIA_FORMAT_I420,
    PJMEDIA_FORMAT_YV12,
    PJMEDIA_FORMAT_YUY2,
    PJMEDIA_FORMAT_RGB24,
    PJMEDIA_FORMAT_RGBA,
    PJMEDIA_FORMAT_BGRA
};

#define NULL_DEV_CAP_IDX  0
#define NULL_DEV_REND_IDX 1
#define NULL_DEV_COUNT    2


struct null_dev_info {
    pjmedia_vid_dev_info info;
};

struct null_factory {
    pjmedia_vid_dev_factory  base;
    pj_pool_t               *pool;
    pj_pool_factory         *pf;
    struct null_dev_info     dev_info[NULL_DEV_COUNT];
};

struct null_stream {
    pjmedia_vid_dev_stream   base;
    pjmedia_vid_dev_param    param;
    pj_pool_t               *pool;
    pj_timestamp             ts;
    unsigned                 ts_inc;
};


/* Factory prototypes */
static pj_status_t null_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t null_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t null_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    null_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t null_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                             unsigned index,
                                             pjmedia_vid_dev_info *info);
static pj_status_t null_factory_default_param(pj_pool_t *pool,
                                              pjmedia_vid_dev_factory *f,
                                              unsigned index,
                                              pjmedia_vid_dev_param *param);
static pj_status_t null_factory_create_stream(
                                pjmedia_vid_dev_factory *f,
                                pjmedia_vid_dev_param *param,
                                const pjmedia_vid_dev_cb *cb,
                                void *user_data,
                                pjmedia_vid_dev_stream **p_vid_strm);

/* Stream prototypes */
static pj_status_t null_stream_get_param(pjmedia_vid_dev_stream *strm,
                                         pjmedia_vid_dev_param *param);
static pj_status_t null_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                       pjmedia_vid_dev_cap cap,
                                       void *value);
static pj_status_t null_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                       pjmedia_vid_dev_cap cap,
                                       const void *value);
static pj_status_t null_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t null_stream_get_frame(pjmedia_vid_dev_stream *strm,
                                         pjmedia_frame *frame);
static pj_status_t null_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                         const pjmedia_frame *frame);
static pj_status_t null_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t null_stream_destroy(pjmedia_vid_dev_stream *strm);


static pjmedia_vid_dev_factory_op factory_op = {
    &null_factory_init,
    &null_factory_destroy,
    &null_factory_get_dev_count,
    &null_factory_get_dev_info,
    &null_factory_default_param,
    &null_factory_create_stream,
    &null_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op = {
    &null_stream_get_param,
    &null_stream_get_cap,
    &null_stream_set_cap,
    &null_stream_start,
    &null_stream_get_frame,
    &null_stream_put_frame,
    &null_stream_stop,
    &null_stream_destroy
};


/*
 * Public entry: create the null video device factory.
 */
pjmedia_vid_dev_factory* pjmedia_null_factory(pj_pool_factory *pf)
{
    struct null_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "null video", 512, 512, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct null_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;
    return &f->base;
}


/* Factory: init */
static pj_status_t null_factory_init(pjmedia_vid_dev_factory *f)
{
    struct null_factory *nf = (struct null_factory*)f;
    struct null_dev_info *di;
    unsigned i;

    /* Capture device */
    di = &nf->dev_info[NULL_DEV_CAP_IDX];
    pj_bzero(di, sizeof(*di));
    pj_ansi_strxcpy(di->info.name, "Null capture", sizeof(di->info.name));
    pj_ansi_strxcpy(di->info.driver, DRIVER_NAME, sizeof(di->info.driver));
    di->info.dir = PJMEDIA_DIR_CAPTURE;
    di->info.has_callback = PJ_FALSE;
    di->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
    di->info.fmt_cnt = PJ_ARRAY_SIZE(null_fmts);
    for (i = 0; i < di->info.fmt_cnt; ++i) {
        pjmedia_format_init_video(&di->info.fmt[i], null_fmts[i],
                                  DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                  DEFAULT_FPS, 1);
    }

    /* Render device */
    di = &nf->dev_info[NULL_DEV_REND_IDX];
    pj_bzero(di, sizeof(*di));
    pj_ansi_strxcpy(di->info.name, "Null renderer", sizeof(di->info.name));
    pj_ansi_strxcpy(di->info.driver, DRIVER_NAME, sizeof(di->info.driver));
    di->info.dir = PJMEDIA_DIR_RENDER;
    di->info.has_callback = PJ_FALSE;
    di->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
    di->info.fmt_cnt = PJ_ARRAY_SIZE(null_fmts);
    for (i = 0; i < di->info.fmt_cnt; ++i) {
        pjmedia_format_init_video(&di->info.fmt[i], null_fmts[i],
                                  DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                  DEFAULT_FPS, 1);
    }

    PJ_LOG(4, (THIS_FILE,
               "Null video initialized with %d device(s):", NULL_DEV_COUNT));
    for (i = 0; i < NULL_DEV_COUNT; ++i) {
        PJ_LOG(4, (THIS_FILE, "%2d: %s", i, nf->dev_info[i].info.name));
    }
    return PJ_SUCCESS;
}

/* Factory: destroy */
static pj_status_t null_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct null_factory *nf = (struct null_factory*)f;
    pj_pool_safe_release(&nf->pool);
    return PJ_SUCCESS;
}

/* Factory: refresh */
static pj_status_t null_factory_refresh(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

/* Factory: get device count */
static unsigned null_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return NULL_DEV_COUNT;
}

/* Factory: get device info */
static pj_status_t null_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                             unsigned index,
                                             pjmedia_vid_dev_info *info)
{
    struct null_factory *nf = (struct null_factory*)f;
    PJ_ASSERT_RETURN(index < NULL_DEV_COUNT, PJMEDIA_EVID_INVDEV);
    pj_memcpy(info, &nf->dev_info[index].info, sizeof(*info));
    return PJ_SUCCESS;
}

/* Factory: create default param */
static pj_status_t null_factory_default_param(pj_pool_t *pool,
                                              pjmedia_vid_dev_factory *f,
                                              unsigned index,
                                              pjmedia_vid_dev_param *param)
{
    struct null_factory *nf = (struct null_factory*)f;
    struct null_dev_info *di;

    PJ_ASSERT_RETURN(index < NULL_DEV_COUNT, PJMEDIA_EVID_INVDEV);
    PJ_UNUSED_ARG(pool);

    di = &nf->dev_info[index];
    pj_bzero(param, sizeof(*param));
    if (index == NULL_DEV_CAP_IDX) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->cap_id = index;
        param->rend_id = PJMEDIA_VID_INVALID_DEV;
    } else {
        param->dir = PJMEDIA_DIR_RENDER;
        param->cap_id = PJMEDIA_VID_INVALID_DEV;
        param->rend_id = index;
    }
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));
    return PJ_SUCCESS;
}

/* Factory: create stream */
static pj_status_t null_factory_create_stream(
                                pjmedia_vid_dev_factory *f,
                                pjmedia_vid_dev_param *param,
                                const pjmedia_vid_dev_cb *cb,
                                void *user_data,
                                pjmedia_vid_dev_stream **p_vid_strm)
{
    struct null_factory *nf = (struct null_factory*)f;
    pj_pool_t *pool;
    struct null_stream *strm;
    const pjmedia_video_format_detail *vfd;
    unsigned i;
    pj_bool_t fmt_supported = PJ_FALSE;

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
                     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
                     (param->dir == PJMEDIA_DIR_CAPTURE ||
                      param->dir == PJMEDIA_DIR_RENDER),
                     PJ_EINVAL);

    if (param->dir == PJMEDIA_DIR_CAPTURE) {
        PJ_ASSERT_RETURN(param->cap_id < NULL_DEV_COUNT &&
                         nf->dev_info[param->cap_id].info.dir ==
                             PJMEDIA_DIR_CAPTURE, PJMEDIA_EVID_INVDEV);
    } else {
        PJ_ASSERT_RETURN(param->rend_id < NULL_DEV_COUNT &&
                         nf->dev_info[param->rend_id].info.dir ==
                             PJMEDIA_DIR_RENDER, PJMEDIA_EVID_INVDEV);
    }

    for (i = 0; i < PJ_ARRAY_SIZE(null_fmts); ++i) {
        if (null_fmts[i] == param->fmt.id) {
            fmt_supported = PJ_TRUE;
            break;
        }
    }
    PJ_ASSERT_RETURN(fmt_supported, PJMEDIA_EVID_BADFORMAT);

    vfd = pjmedia_format_get_video_format_detail(&param->fmt, PJ_TRUE);
    PJ_UNUSED_ARG(cb);
    PJ_UNUSED_ARG(user_data);

    pool = pj_pool_create(nf->pf, "null-dev", 256, 256, NULL);
    if (!pool) return PJ_ENOMEM;

    strm = PJ_POOL_ZALLOC_T(pool, struct null_stream);
    strm->pool = pool;
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    strm->base.op = &stream_op;

    *p_vid_strm = &strm->base;
    return PJ_SUCCESS;
}


/* Stream: get param */
static pj_status_t null_stream_get_param(pjmedia_vid_dev_stream *s,
                                         pjmedia_vid_dev_param *pi)
{
    struct null_stream *strm = (struct null_stream*)s;
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    pj_memcpy(pi, &strm->param, sizeof(*pi));
    return PJ_SUCCESS;
}

/* Stream: get cap */
static pj_status_t null_stream_get_cap(pjmedia_vid_dev_stream *s,
                                       pjmedia_vid_dev_cap cap,
                                       void *value)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(value);
    return PJMEDIA_EVID_INVCAP;
}

/* Stream: set cap */
static pj_status_t null_stream_set_cap(pjmedia_vid_dev_stream *s,
                                       pjmedia_vid_dev_cap cap,
                                       const void *value)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(value);
    return PJMEDIA_EVID_INVCAP;
}

/* Stream: start */
static pj_status_t null_stream_start(pjmedia_vid_dev_stream *s)
{
    PJ_UNUSED_ARG(s);
    return PJ_SUCCESS;
}

/* Stream: get frame (capture) — return a zero/black frame */
static pj_status_t null_stream_get_frame(pjmedia_vid_dev_stream *s,
                                         pjmedia_frame *frame)
{
    struct null_stream *strm = (struct null_stream*)s;
    pj_status_t status;
    frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
    frame->bit_info = 0;
    frame->timestamp = strm->ts;
    strm->ts.u64 += strm->ts_inc;
    if (frame->buf && frame->size) {
        status = pjmedia_video_format_fill_black(&strm->param.fmt,
                                                 frame->buf,
                                                 frame->size);
        if (status != PJ_SUCCESS)
            return status;
    }
    return PJ_SUCCESS;
}

/* Stream: put frame (renderer) — discard */
static pj_status_t null_stream_put_frame(pjmedia_vid_dev_stream *s,
                                         const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}

/* Stream: stop */
static pj_status_t null_stream_stop(pjmedia_vid_dev_stream *s)
{
    PJ_UNUSED_ARG(s);
    return PJ_SUCCESS;
}

/* Stream: destroy */
static pj_status_t null_stream_destroy(pjmedia_vid_dev_stream *s)
{
    struct null_stream *strm = (struct null_stream*)s;
    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);
    pj_pool_release(strm->pool);
    return PJ_SUCCESS;
}


#endif  /* PJMEDIA_VIDEO_DEV_HAS_NULL */

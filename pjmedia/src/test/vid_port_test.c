/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
#include "test.h"
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia-codec/ffmpeg_vid_codecs.h>
#include <pjmedia/vid_codec.h>
#include <pjmedia_videodev.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define THIS_FILE       "vid_dev_test.c"
#define LOOP_DURATION   6

static pj_bool_t is_quitting = PJ_FALSE;

static pj_status_t vid_event_cb(pjmedia_event *event,
                                void *user_data)
{
    PJ_UNUSED_ARG(user_data);

    if (event->type == PJMEDIA_EVENT_WND_CLOSED)
        is_quitting = PJ_TRUE;

    return PJ_SUCCESS;
}

static int capture_render_loopback(pj_bool_t active,
                                   int cap_dev_id, int rend_dev_id,
                                   const pjmedia_format *fmt)
{
    pj_pool_t *pool;
    pjmedia_vid_port *capture=NULL, *renderer=NULL;
    pjmedia_vid_dev_info cdi, rdi;
    pjmedia_vid_port_param param;
    pjmedia_video_format_detail *vfd;
    pj_status_t status;
    int rc = 0, i;

    pool = pj_pool_create(mem, "vidportloop", 1000, 1000, NULL);

    status = pjmedia_vid_dev_get_info(cap_dev_id, &cdi);
    if (status != PJ_SUCCESS)
        goto on_return;

    status = pjmedia_vid_dev_get_info(rend_dev_id, &rdi);
    if (status != PJ_SUCCESS)
        goto on_return;

    PJ_LOG(3,(THIS_FILE,
              "  %s (%s) ===> %s (%s)\t%s\t%dx%d\t@%d:%d fps",
              cdi.name, cdi.driver, rdi.name, rdi.driver,
              pjmedia_get_video_format_info(NULL, fmt->id)->name,
              fmt->det.vid.size.w, fmt->det.vid.size.h,
              fmt->det.vid.fps.num, fmt->det.vid.fps.denum));

    pjmedia_vid_port_param_default(&param);

    /* Create capture, set it to active (master) */
    status = pjmedia_vid_dev_default_param(pool, cap_dev_id,
                                           &param.vidparam);
    if (status != PJ_SUCCESS) {
        rc = 100; goto on_return;
    }
    param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
    param.vidparam.fmt = *fmt;
    param.active = (active? PJ_TRUE: PJ_FALSE);

    if (param.vidparam.fmt.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO) {
        rc = 103; goto on_return;
    }

    vfd = pjmedia_format_get_video_format_detail(&param.vidparam.fmt, PJ_TRUE);
    if (vfd == NULL) {
        rc = 105; goto on_return;
    }

    status = pjmedia_vid_port_create(pool, &param, &capture);
    if (status != PJ_SUCCESS) {
        rc = 110; goto on_return;
    }

    /* Create renderer, set it to passive (slave)  */
    status = pjmedia_vid_dev_default_param(pool, rend_dev_id,
                                           &param.vidparam);
    if (status != PJ_SUCCESS) {
        rc = 120; goto on_return;
    }

    param.active = (active? PJ_FALSE: PJ_TRUE);
    param.vidparam.dir = PJMEDIA_DIR_RENDER;
    param.vidparam.rend_id = rend_dev_id;
    param.vidparam.fmt = *fmt;
    param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(pool, &param, &renderer);
    if (status != PJ_SUCCESS) {
        rc = 130; goto on_return;
    }

    /* Set event handler */
    pjmedia_event_subscribe(NULL, &vid_event_cb, NULL, renderer);

    /* Connect capture to renderer */
    status = pjmedia_vid_port_connect(
                 (active? capture: renderer),
                 pjmedia_vid_port_get_passive_port(active? renderer: capture),
                 PJ_FALSE);
    if (status != PJ_SUCCESS) {
        rc = 140; goto on_return;
    }

    /* Start streaming.. */
    status = pjmedia_vid_port_start(renderer);
    if (status != PJ_SUCCESS) {
        rc = 150; goto on_return;
    }
    status = pjmedia_vid_port_start(capture);
    if (status != PJ_SUCCESS) {
        rc = 160; goto on_return;
    }

    /* Sleep while the webcam is being displayed... */
    for (i = 0; i < LOOP_DURATION*10 && (!is_quitting); i++) {
        pj_thread_sleep(100);
    }

on_return:
    if (status != PJ_SUCCESS)
        PJ_PERROR(3, (THIS_FILE, status, "   error"));

    if (capture)
        pjmedia_vid_port_stop(capture);
    if (renderer)
        pjmedia_vid_port_stop(renderer);
    if (capture)
        pjmedia_vid_port_destroy(capture);
    if (renderer) {
        pjmedia_event_unsubscribe(NULL, &vid_event_cb, NULL, renderer);
        pjmedia_vid_port_destroy(renderer);
    }

    pj_pool_release(pool);
    return rc;
}

static int find_device(pjmedia_dir dir,
                       pj_bool_t has_callback)
{
    unsigned i, count = pjmedia_vid_dev_count();
 
    for (i = 0; i < count; ++i) {
        pjmedia_vid_dev_info cdi;

        if (pjmedia_vid_dev_get_info(i, &cdi) != PJ_SUCCESS)
            continue;
        if ((cdi.dir & dir) != 0 && cdi.has_callback == has_callback)
            return i;
    }
    
    return -999;
}

static int vidport_test(void)
{
    int i, j, k, l;
    int cap_id, rend_id;
    pjmedia_format_id test_fmts[] = {
        PJMEDIA_FORMAT_RGBA,
        PJMEDIA_FORMAT_I420
    };

    PJ_LOG(3, (THIS_FILE, " Video port tests:"));

    /* Capturer's role: active/passive. */
    for (i = 1; i >= 0; i--) {
        /* Capturer's device has_callback: TRUE/FALSE. */
        for (j = 1; j >= 0; j--) {
            cap_id = find_device(PJMEDIA_DIR_CAPTURE, j);
            if (cap_id < 0)
                continue;

            /* Renderer's device has callback: TRUE/FALSE. */
            for (k = 1; k >= 0; k--) {
                rend_id = find_device(PJMEDIA_DIR_RENDER, k);
                if (rend_id < 0)
                    continue;

                /* Check various formats to test format conversion. */
                for (l = 0; l < (int)PJ_ARRAY_SIZE(test_fmts); ++l) {
                    pjmedia_format fmt;

                    PJ_LOG(3,(THIS_FILE,
                              "capturer %s (stream: %s) ===> "
                              "renderer %s (stream: %s)",
                              (i? "active": "passive"),
                              (j? "active": "passive"),
                              (i? "passive": "active"),
                              (k? "active": "passive")));

                    pjmedia_format_init_video(&fmt, test_fmts[l],
                                              640, 480, 25, 1);
                    capture_render_loopback(i, cap_id, rend_id, &fmt);
                }
            }
        }
    }

    return 0;
}

/* Format change test: a passive render device that only takes BGRA, so the
 * video port converts, fed by a source whose frame size grows.
 */
#if (defined(PJMEDIA_HAS_LIBYUV) && PJMEDIA_HAS_LIBYUV != 0) || \
    (defined(PJMEDIA_HAS_LIBSWSCALE) && PJMEDIA_HAS_LIBSWSCALE != 0)
#   define HAS_FMT_CHANGE_TEST  1
#else
#   define HAS_FMT_CHANGE_TEST  0
#endif

#if HAS_FMT_CHANGE_TEST
#define FC_W1           160
#define FC_H1           120
#define FC_W2           640
#define FC_H2           480
#define FC_FPS          50

typedef struct fc_stream
{
    pjmedia_vid_dev_stream   base;
    pjmedia_vid_dev_param    param;
    pj_atomic_t             *frames;
    pj_atomic_t             *big_frames;
} fc_stream;

typedef struct fc_src_port
{
    pjmedia_port             base;
    pj_atomic_t             *big;
} fc_src_port;

static pjmedia_vid_dev_factory fc_factory;
static fc_stream *fc_strm;

static pj_status_t fc_factory_init(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

static pj_status_t fc_factory_destroy(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

static unsigned fc_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return 1;
}

static pj_status_t fc_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                           unsigned index,
                                           pjmedia_vid_dev_info *info)
{
    PJ_UNUSED_ARG(f);
    PJ_ASSERT_RETURN(index == 0, PJMEDIA_EVID_INVDEV);

    pj_bzero(info, sizeof(*info));
    pj_ansi_strxcpy(info->name, "fmt-change", sizeof(info->name));
    pj_ansi_strxcpy(info->driver, "test", sizeof(info->driver));
    info->dir = PJMEDIA_DIR_RENDER;
    info->has_callback = PJ_FALSE;
    info->caps = PJMEDIA_VID_DEV_CAP_FORMAT;
    info->fmt_cnt = 1;
    pjmedia_format_init_video(&info->fmt[0], PJMEDIA_FORMAT_BGRA,
                              FC_W1, FC_H1, FC_FPS, 1);
    return PJ_SUCCESS;
}

static pj_status_t fc_factory_default_param(pj_pool_t *pool,
                                            pjmedia_vid_dev_factory *f,
                                            unsigned index,
                                            pjmedia_vid_dev_param *param)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(f);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_RENDER;
    param->cap_id = PJMEDIA_VID_INVALID_DEV;
    param->rend_id = index;
    param->clock_rate = 90000;
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    pjmedia_format_init_video(&param->fmt, PJMEDIA_FORMAT_BGRA,
                              FC_W1, FC_H1, FC_FPS, 1);
    return PJ_SUCCESS;
}

static pj_status_t fc_stream_get_param(pjmedia_vid_dev_stream *s,
                                       pjmedia_vid_dev_param *param)
{
    pj_memcpy(param, &((fc_stream*)s)->param, sizeof(*param));
    return PJ_SUCCESS;
}

static pj_status_t fc_stream_get_cap(pjmedia_vid_dev_stream *s,
                                     pjmedia_vid_dev_cap cap,
                                     void *value)
{
    if (cap != PJMEDIA_VID_DEV_CAP_FORMAT)
        return PJMEDIA_EVID_INVCAP;
    pjmedia_format_copy((pjmedia_format*)value, &((fc_stream*)s)->param.fmt);
    return PJ_SUCCESS;
}

static pj_status_t fc_stream_set_cap(pjmedia_vid_dev_stream *s,
                                     pjmedia_vid_dev_cap cap,
                                     const void *value)
{
    if (cap != PJMEDIA_VID_DEV_CAP_FORMAT)
        return PJMEDIA_EVID_INVCAP;
    pjmedia_format_copy(&((fc_stream*)s)->param.fmt,
                        (const pjmedia_format*)value);
    return PJ_SUCCESS;
}

static pj_status_t fc_stream_start_stop(pjmedia_vid_dev_stream *s)
{
    PJ_UNUSED_ARG(s);
    return PJ_SUCCESS;
}

static pj_status_t fc_stream_get_frame(pjmedia_vid_dev_stream *s,
                                       pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(frame);
    return PJ_ENOTSUP;
}

static pj_status_t fc_stream_put_frame(pjmedia_vid_dev_stream *s,
                                       const pjmedia_frame *frame)
{
    fc_stream *strm = (fc_stream*)s;
    const pj_uint8_t *last;

    if (frame->type != PJMEDIA_FRAME_TYPE_VIDEO || !frame->buf)
        return PJ_SUCCESS;

    pj_atomic_inc(strm->frames);
    if (frame->size != FC_W2 * FC_H2 * 4)
        return PJ_SUCCESS;

    /* The last pixel must come from the source (gray), not black */
    last = (const pj_uint8_t*)frame->buf + frame->size - 4;
    if (last[0] > 0x60 && last[0] < 0xA0)
        pj_atomic_inc(strm->big_frames);
    return PJ_SUCCESS;
}

static pjmedia_vid_dev_stream_op fc_stream_op =
{
    &fc_stream_get_param,
    &fc_stream_get_cap,
    &fc_stream_set_cap,
    &fc_stream_start_stop,
    &fc_stream_get_frame,
    &fc_stream_put_frame,
    &fc_stream_start_stop,
    &fc_stream_start_stop
};

static pj_status_t fc_factory_create_stream(pjmedia_vid_dev_factory *f,
                                            pjmedia_vid_dev_param *param,
                                            const pjmedia_vid_dev_cb *cb,
                                            void *user_data,
                                            pjmedia_vid_dev_stream **p_strm)
{
    PJ_UNUSED_ARG(f);
    PJ_UNUSED_ARG(cb);
    PJ_UNUSED_ARG(user_data);

    PJ_ASSERT_RETURN(fc_strm, PJ_EBUG);
    pj_memcpy(&fc_strm->param, param, sizeof(*param));
    fc_strm->base.op = &fc_stream_op;
    *p_strm = &fc_strm->base;
    return PJ_SUCCESS;
}

static pj_status_t fc_factory_refresh(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

static pjmedia_vid_dev_factory_op fc_factory_op =
{
    &fc_factory_init,
    &fc_factory_destroy,
    &fc_factory_get_dev_count,
    &fc_factory_get_dev_info,
    &fc_factory_default_param,
    &fc_factory_create_stream,
    &fc_factory_refresh
};

static pj_status_t fc_src_get_frame(pjmedia_port *port,
                                    pjmedia_frame *frame)
{
    fc_src_port *src = (fc_src_port*)port;
    pj_size_t needed = pj_atomic_get(src->big)? FC_W2 * FC_H2 * 3 / 2 :
                                                FC_W1 * FC_H1 * 3 / 2;

    if (frame->size < needed) {
        frame->type = PJMEDIA_FRAME_TYPE_NONE;
        frame->size = 0;
        return PJ_SUCCESS;
    }

    pj_memset(frame->buf, 0x80, needed);
    frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
    frame->size = needed;
    return PJ_SUCCESS;
}

static pj_bool_t fc_wait(pj_atomic_t *cnt)
{
    unsigned i;

    for (i = 0; i < 500 && pj_atomic_get(cnt) == 0; ++i)
        pj_thread_sleep(10);
    return pj_atomic_get(cnt) != 0;
}

/* After the source grows, the active render port must deliver
 * full-size converted frames to the passive device.
 */
static int vid_port_fmt_change_test(void)
{
    pj_pool_t *pool;
    pjmedia_vid_port *vp = NULL;
    pjmedia_vid_port_param param;
    pjmedia_vid_dev_index id;
    fc_src_port *src;
    pjmedia_event ev;
    pj_str_t name = pj_str("fc-src");
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, " Video port format change test"));

    pool = pj_pool_create(mem, "vidportfc", 1000, 1000, NULL);
    fc_strm = PJ_POOL_ZALLOC_T(pool, fc_stream);
    src = PJ_POOL_ZALLOC_T(pool, fc_src_port);
    if (pj_atomic_create(pool, 0, &fc_strm->frames) != PJ_SUCCESS ||
        pj_atomic_create(pool, 0, &fc_strm->big_frames) != PJ_SUCCESS ||
        pj_atomic_create(pool, 0, &src->big) != PJ_SUCCESS)
    {
        rc = -90;
        goto on_return;
    }
    fc_factory.op = &fc_factory_op;

    status = pjmedia_vid_register_factory(NULL, &fc_factory);
    if (status != PJ_SUCCESS) {
        rc = -100;
        goto on_return;
    }

    status = pjmedia_vid_dev_lookup("test", "fmt-change", &id);
    if (status != PJ_SUCCESS) {
        rc = -110;
        goto on_unregister;
    }

    src->base.get_frame = &fc_src_get_frame;

    pjmedia_vid_port_param_default(&param);
    status = pjmedia_vid_dev_default_param(pool, id, &param.vidparam);
    if (status != PJ_SUCCESS) {
        rc = -120;
        goto on_unregister;
    }
    param.active = PJ_TRUE;
    param.vidparam.dir = PJMEDIA_DIR_RENDER;
    pjmedia_format_init_video(&param.vidparam.fmt, PJMEDIA_FORMAT_I420,
                              FC_W1, FC_H1, FC_FPS, 1);
    pjmedia_port_info_init2(&src->base.info, &name, PJMEDIA_SIG_VID_PORT,
                            PJMEDIA_DIR_DECODING, &param.vidparam.fmt);

    status = pjmedia_vid_port_create(pool, &param, &vp);
    if (status != PJ_SUCCESS) {
        rc = -130;
        goto on_unregister;
    }

    status = pjmedia_vid_port_connect(vp, &src->base, PJ_FALSE);
    if (status != PJ_SUCCESS) {
        rc = -140;
        goto on_destroy;
    }

    status = pjmedia_vid_port_start(vp);
    if (status != PJ_SUCCESS) {
        rc = -150;
        goto on_destroy;
    }
    if (!fc_wait(fc_strm->frames)) {
        PJ_LOG(3, (THIS_FILE, "  no frame rendered"));
        rc = -155;
        goto on_stop;
    }

    pj_atomic_set(src->big, 1);
    pjmedia_event_init(&ev, PJMEDIA_EVENT_FMT_CHANGED, NULL, &src->base);
    ev.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
    pjmedia_format_init_video(&ev.data.fmt_changed.new_fmt,
                              PJMEDIA_FORMAT_I420, FC_W2, FC_H2, FC_FPS, 1);
    status = pjmedia_event_publish(NULL, &src->base, &ev, 0);
    if (status != PJ_SUCCESS) {
        rc = -158;
        goto on_stop;
    }

    if (!fc_wait(fc_strm->big_frames)) {
        PJ_LOG(3, (THIS_FILE, "  no full-size frame rendered after change"));
        rc = -160;
    }

on_stop:
    pjmedia_vid_port_stop(vp);
on_destroy:
    pjmedia_vid_port_destroy(vp);
on_unregister:
    pjmedia_vid_unregister_factory(NULL, &fc_factory);
on_return:
    if (fc_strm->frames)
        pj_atomic_destroy(fc_strm->frames);
    if (fc_strm->big_frames)
        pj_atomic_destroy(fc_strm->big_frames);
    if (src->big)
        pj_atomic_destroy(src->big);
    fc_strm = NULL;
    pj_pool_release(pool);
    return rc;
}
#endif /* HAS_FMT_CHANGE_TEST */

int vid_port_test(void)
{
    int rc = 0;
    pj_status_t status;

    status = pjmedia_vid_dev_subsys_init(mem);
    if (status != PJ_SUCCESS)
        return -10;

#if HAS_FMT_CHANGE_TEST
    rc = vid_port_fmt_change_test();
    if (rc != 0)
        goto on_return;
#endif

    rc = vidport_test();
    if (rc != 0)
        goto on_return;

on_return:
    pjmedia_vid_dev_subsys_shutdown();

    return rc;
}


#endif /* PJMEDIA_HAS_VIDEO */

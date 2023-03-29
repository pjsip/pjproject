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

/* Video device with ffmpeg backend, currently only capture devices are
 * implemented.
 *
 * Issues:
 * - no device enumeration (ffmpeg limitation), so this uses "host API" enum
 *   instead
 * - need stricter filter on "host API" enum, currently audio capture devs are
 *   still listed.
 * - no format enumeration, currently hardcoded to PJMEDIA_FORMAT_RGB24 only
 * - tested on Vista only (vfw backend) with virtual cam
 * - vfw backend produce bottom up pictures
 * - using VS IDE, this cannot run under debugger!
 */

#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/unicode.h>


#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_HAS_LIBAVDEVICE) && PJMEDIA_HAS_LIBAVDEVICE != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_FFMPEG) && PJMEDIA_VIDEO_DEV_HAS_FFMPEG != 0


#define THIS_FILE               "ffmpeg.c"

#define LIBAVFORMAT_VER_AT_LEAST(major,minor)  (LIBAVFORMAT_VERSION_MAJOR > major || \
                                               (LIBAVFORMAT_VERSION_MAJOR == major && \
                                                LIBAVFORMAT_VERSION_MINOR >= minor))

#include "../pjmedia/ffmpeg_util.h"
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#if LIBAVFORMAT_VER_AT_LEAST(53,2)
# include <libavutil/pixdesc.h>
#endif

#define MAX_DEV_CNT     8

#ifndef PJMEDIA_USE_OLD_FFMPEG
#  define av_close_input_stream(ctx) avformat_close_input(&ctx)
#endif


typedef struct ffmpeg_dev_info
{
    pjmedia_vid_dev_info         base;
    AVInputFormat               *host_api;
    const char                  *def_devname;
} ffmpeg_dev_info;


typedef struct ffmpeg_factory
{
    pjmedia_vid_dev_factory      base;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
    pj_pool_t                   *dev_pool;
    unsigned                     dev_count;
    ffmpeg_dev_info              dev_info[MAX_DEV_CNT];
} ffmpeg_factory;


typedef struct ffmpeg_stream
{
    pjmedia_vid_dev_stream       base;
    ffmpeg_factory              *factory;
    pj_pool_t                   *pool;
    pjmedia_vid_dev_param        param;
    AVFormatContext             *ff_fmt_ctx;
    void                        *frame_buf;
} ffmpeg_stream;


/* Prototypes */
static pj_status_t ffmpeg_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t ffmpeg_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t ffmpeg_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    ffmpeg_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t ffmpeg_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_info *info);
static pj_status_t ffmpeg_factory_default_param(pj_pool_t *pool,
                                                pjmedia_vid_dev_factory *f,
                                                unsigned index,
                                                pjmedia_vid_dev_param *param);
static pj_status_t ffmpeg_factory_create_stream(
                                        pjmedia_vid_dev_factory *f,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t ffmpeg_stream_get_param(pjmedia_vid_dev_stream *strm,
                                           pjmedia_vid_dev_param *param);
static pj_status_t ffmpeg_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                         pjmedia_vid_dev_cap cap,
                                         void *value);
static pj_status_t ffmpeg_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                         pjmedia_vid_dev_cap cap,
                                         const void *value);
static pj_status_t ffmpeg_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t ffmpeg_stream_get_frame(pjmedia_vid_dev_stream *s,
                                           pjmedia_frame *frame);
static pj_status_t ffmpeg_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t ffmpeg_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &ffmpeg_factory_init,
    &ffmpeg_factory_destroy,
    &ffmpeg_factory_get_dev_count,
    &ffmpeg_factory_get_dev_info,
    &ffmpeg_factory_default_param,
    &ffmpeg_factory_create_stream,
    &ffmpeg_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
    &ffmpeg_stream_get_param,
    &ffmpeg_stream_get_cap,
    &ffmpeg_stream_set_cap,
    &ffmpeg_stream_start,
    &ffmpeg_stream_get_frame,
    NULL,
    &ffmpeg_stream_stop,
    &ffmpeg_stream_destroy
};


static void print_ffmpeg_err(int err)
{
    char errbuf[512];
    if (av_strerror(err, errbuf, sizeof(errbuf)) >= 0)
        PJ_LOG(1, (THIS_FILE, "ffmpeg err %d: %s", err, errbuf));

}

static void print_ffmpeg_log(void* ptr, int level, const char* fmt, va_list vl)
{
    PJ_UNUSED_ARG(ptr);

    /* Custom callback needs to filter log level by itself */
    if (level > av_log_get_level())
        return;

    vfprintf(stdout, fmt, vl);
}


static pj_status_t ffmpeg_capture_open(AVFormatContext **ctx,
                                       AVInputFormat *ifmt,
                                       const char *dev_name,
                                       const pjmedia_vid_dev_param *param)
{
#if LIBAVFORMAT_VER_AT_LEAST(53,2)
    AVDictionary *format_opts = NULL;
    char buf[128];
    enum AVPixelFormat av_fmt;
#else
    AVFormatParameters fp;
#endif
    pjmedia_video_format_detail *vfd;
    pj_status_t status;
    int err;

    PJ_ASSERT_RETURN(ctx && ifmt && dev_name && param, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO,
                     PJ_EINVAL);

    status = pjmedia_format_id_to_PixelFormat(param->fmt.id, &av_fmt);
    if (status != PJ_SUCCESS) {
        avformat_free_context(*ctx);
        return status;
    }

    vfd = pjmedia_format_get_video_format_detail(&param->fmt, PJ_TRUE);

    /* Init ffmpeg format context */
    *ctx = avformat_alloc_context();

#if LIBAVFORMAT_VER_AT_LEAST(53,2)
    /* Init ffmpeg dictionary */
    /*
    snprintf(buf, sizeof(buf), "%d/%d", vfd->fps.num, vfd->fps.denum);
    av_dict_set(&format_opts, "framerate", buf, 0);
    snprintf(buf, sizeof(buf), "%dx%d", vfd->size.w, vfd->size.h);
    av_dict_set(&format_opts, "video_size", buf, 0);
    av_dict_set(&format_opts, "pixel_format", av_get_pix_fmt_name(av_fmt), 0);
    */
    /* Open capture stream */
    err = avformat_open_input(ctx, dev_name, ifmt, &format_opts);
#else
    /* Init ffmpeg format param */
    pj_bzero(&fp, sizeof(fp));
    fp.prealloced_context = 1;
    fp.width = vfd->size.w;
    fp.height = vfd->size.h;
    fp.pix_fmt = av_fmt;
    fp.time_base.num = vfd->fps.denum;
    fp.time_base.den = vfd->fps.num;

    /* Open capture stream */
    err = av_open_input_stream(ctx, NULL, dev_name, ifmt, &fp);
#endif
    if (err < 0) {
        *ctx = NULL; /* ffmpeg freed its states on failure, do we must too */
        print_ffmpeg_err(err);
        return PJ_EUNKNOWN;
    }

    return PJ_SUCCESS;
}

static void ffmpeg_capture_close(AVFormatContext *ctx)
{
    if (ctx)
        av_close_input_stream(ctx);
}


/****************************************************************************
 * Factory operations
 */
/*
 * Init ffmpeg_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_ffmpeg_factory(pj_pool_factory *pf)
{
    ffmpeg_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "ffmpeg_cap_dev", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, ffmpeg_factory);

    f->pool = pool;
    f->pf = pf;
    f->base.op = &factory_op;

    avdevice_register_all();

    return &f->base;
}


/* API: init factory */
static pj_status_t ffmpeg_factory_init(pjmedia_vid_dev_factory *f)
{
    return ffmpeg_factory_refresh(f);
}

/* API: destroy factory */
static pj_status_t ffmpeg_factory_destroy(pjmedia_vid_dev_factory *f)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;
    pj_pool_t *pool = ff->pool;

    ff->dev_count = 0;
    ff->pool = NULL;
    if (ff->dev_pool)
        pj_pool_release(ff->dev_pool);
    if (pool)
        pj_pool_release(pool);

    return PJ_SUCCESS;
}


#if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
    (defined(PJ_WIN64) && PJ_WIN64!=0)

#ifdef _MSC_VER
#   pragma warning(push, 3)
#endif

#define COBJMACROS
#include <DShow.h>
#pragma comment(lib, "Strmiids.lib")

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#define MAX_DEV_NAME_LEN 80

static pj_status_t dshow_enum_devices(unsigned *dev_cnt,
                                      char dev_names[][MAX_DEV_NAME_LEN])
{
    unsigned max_cnt = *dev_cnt;
    ICreateDevEnum *dev_enum = NULL;
    IEnumMoniker *enum_cat = NULL;
    IMoniker *moniker = NULL;
    HRESULT hr;
    ULONG fetched;
    unsigned i = 0;

    CoInitialize(0);

    *dev_cnt = 0;
    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL,
                          CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum,
                          (void**)&dev_enum);
    if (FAILED(hr) ||
        ICreateDevEnum_CreateClassEnumerator(dev_enum,
            &CLSID_VideoInputDeviceCategory, &enum_cat, 0) != S_OK) 
    {
        PJ_LOG(4,(THIS_FILE, "Windows found no video input devices"));
        if (dev_enum)
            ICreateDevEnum_Release(dev_enum);

        return PJ_SUCCESS;
    }

    while (IEnumMoniker_Next(enum_cat, 1, &moniker, &fetched) == S_OK &&
           *dev_cnt < max_cnt)
    {
        (*dev_cnt)++;
    }

    if (*dev_cnt == 0) {
        IEnumMoniker_Release(enum_cat);
        ICreateDevEnum_Release(dev_enum);
        return PJ_SUCCESS;
    }

    IEnumMoniker_Reset(enum_cat);
    while (i < max_cnt &&
           IEnumMoniker_Next(enum_cat, 1, &moniker, &fetched) == S_OK)
    {
        IPropertyBag *prop_bag;

        hr = IMoniker_BindToStorage(moniker, 0, 0, &IID_IPropertyBag,
                                    (void**)&prop_bag);
        if (SUCCEEDED(hr)) {
            VARIANT var_name;

            VariantInit(&var_name);
            hr = IPropertyBag_Read(prop_bag, L"FriendlyName", &var_name,
                                   NULL);
            if (SUCCEEDED(hr) && var_name.bstrVal) {
                char tmp[MAX_DEV_NAME_LEN] = {0};
                WideCharToMultiByte(CP_ACP, 0, var_name.bstrVal,
                                    (int)wcslen(var_name.bstrVal),
                                    tmp, MAX_DEV_NAME_LEN, NULL, NULL);
                pj_ansi_snprintf(dev_names[i++], MAX_DEV_NAME_LEN,
                                 "video=%s", tmp);
            }
            VariantClear(&var_name);
            IPropertyBag_Release(prop_bag);
        }
        IMoniker_Release(moniker);
    }

    IEnumMoniker_Release(enum_cat);
    ICreateDevEnum_Release(dev_enum);

    PJ_LOG(4, (THIS_FILE, "DShow has %d devices:", *dev_cnt));
    for (i = 0; i < *dev_cnt; ++i) {
        PJ_LOG(4, (THIS_FILE, " %d: %s", (i+1), dev_names[i]));
    }

    return PJ_SUCCESS;
}

#endif /* PJ_WIN32 or PJ_WIN64 */


/* API: refresh the list of devices */
static pj_status_t ffmpeg_factory_refresh(pjmedia_vid_dev_factory *f)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;
    AVInputFormat *p;

    av_log_set_callback(&print_ffmpeg_log);
    av_log_set_level(AV_LOG_ERROR);

    if (ff->dev_pool) {
        pj_pool_release(ff->dev_pool);
        ff->dev_pool = NULL;
    }

    ff->dev_count = 0;
    ff->dev_pool = pj_pool_create(ff->pf, "ffmpeg_cap_dev", 500, 500, NULL);

    /* Iterate host APIs */
    p = av_input_video_device_next(NULL);
    while (p && ff->dev_count < MAX_DEV_CNT) {
        char dev_names[MAX_DEV_CNT][MAX_DEV_NAME_LEN];
        unsigned dev_cnt = MAX_DEV_CNT;
        unsigned dev_idx;

        if ((p->flags & AVFMT_NOFILE)==0 || p->read_probe) {
            goto next_format;
        }

#if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
    (defined(PJ_WIN64) && PJ_WIN64!=0)
        if (pj_ansi_strcmp(p->name, "dshow") == 0) {
            dshow_enum_devices(&dev_cnt, dev_names);
        } else if (pj_ansi_strcmp(p->name, "vfwcap") == 0) {
            dev_cnt = 1;
            pj_ansi_snprintf(dev_names[0], MAX_DEV_NAME_LEN, "0");
        } else {
            dev_cnt = 0;
        }
#elif defined(PJ_LINUX) && PJ_LINUX!=0
        dev_cnt = 1;
        pj_ansi_snprintf(dev_names[0], MAX_DEV_NAME_LEN, "/dev/video0");
#else
        dev_cnt = 0;
#endif

        /* Iterate devices (only DirectShow devices for now) */
        for (dev_idx = 0; dev_idx < dev_cnt && ff->dev_count < MAX_DEV_CNT;
            ++dev_idx)
        {
            ffmpeg_dev_info *info;
            AVFormatContext *ctx;
            AVCodecContext *codec = NULL;
            pjmedia_format_id fmt_id;
            pj_str_t dev_name;
            pj_status_t status;
            unsigned i;
            
            ctx = avformat_alloc_context();
            if (!ctx || avformat_open_input(&ctx, dev_names[dev_idx], p, NULL)!=0)
                continue;

            for(i = 0; i < ctx->nb_streams; i++) {
                if (ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                    codec = ctx->streams[i]->codec;
                    break;
                }
            }
            if (!codec) {
                av_close_input_stream(ctx);
                continue;
            }

            status = PixelFormat_to_pjmedia_format_id(codec->pix_fmt, &fmt_id);
            if (status != PJ_SUCCESS) {
                av_close_input_stream(ctx);
                continue;
            }

            info = &ff->dev_info[ff->dev_count++];
            pj_bzero(info, sizeof(*info));
            pj_ansi_strxcpy(info->base.name, "default", 
                            sizeof(info->base.name));
            pj_ansi_snprintf(info->base.driver, sizeof(info->base.driver),
                             "ffmpeg %s", p->name);
            
            pj_strdup2_with_null(ff->pool, &dev_name, dev_names[dev_idx]);
            info->def_devname = dev_name.ptr;
            info->base.dir = PJMEDIA_DIR_CAPTURE;
            info->base.has_callback = PJ_FALSE;

            info->host_api = p;

            /* Set supported formats */
            info->base.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
            info->base.fmt_cnt = 1;
            for (i = 0; i < info->base.fmt_cnt; ++i) {
                pjmedia_format *fmt = &info->base.fmt[i];
                pjmedia_format_init_video(fmt, fmt_id,
                                          codec->width, codec->height, 15, 1);
            }

            av_close_input_stream(ctx);
        }

next_format:
        p = av_input_video_device_next(p);
    }

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned ffmpeg_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;
    return ff->dev_count;
}

/* API: get device info */
static pj_status_t ffmpeg_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_info *info)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;

    PJ_ASSERT_RETURN(index < ff->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &ff->dev_info[index].base, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t ffmpeg_factory_default_param(pj_pool_t *pool,
                                                pjmedia_vid_dev_factory *f,
                                                unsigned index,
                                                pjmedia_vid_dev_param *param)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;
    ffmpeg_dev_info *info;

    PJ_ASSERT_RETURN(index < ff->dev_count, PJMEDIA_EVID_INVDEV);

    PJ_UNUSED_ARG(pool);

    info = &ff->dev_info[index];

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE;
    param->cap_id = index;
    param->rend_id = PJMEDIA_VID_INVALID_DEV;
    param->clock_rate = 0;

    /* Set the device capabilities here */
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = 90000;
    pjmedia_format_init_video(&param->fmt, 0, 320, 240, 25, 1);
    param->fmt.id = info->base.fmt[0].id;

    return PJ_SUCCESS;
}



/* API: create stream */
static pj_status_t ffmpeg_factory_create_stream(
                                        pjmedia_vid_dev_factory *f,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm)
{
    ffmpeg_factory *ff = (ffmpeg_factory*)f;
    pj_pool_t *pool;
    ffmpeg_stream *strm;

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->dir == PJMEDIA_DIR_CAPTURE, PJ_EINVAL);
    PJ_ASSERT_RETURN((unsigned)param->cap_id < ff->dev_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO,
                     PJ_EINVAL);

    PJ_UNUSED_ARG(cb);
    PJ_UNUSED_ARG(user_data);

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(ff->pf, "ffmpeg-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct ffmpeg_stream);
    strm->factory = (ffmpeg_factory*)f;
    strm->pool = pool;
    pj_memcpy(&strm->param, param, sizeof(*param));

    /* Allocate frame buffer */
    {
        const pjmedia_video_format_info *vfi;
        pjmedia_video_apply_fmt_param vafp;

        vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
        if (!vfi) goto on_error;

        pj_bzero(&vafp, sizeof(vafp));
        vafp.size = param->fmt.det.vid.size;
        vfi->apply_fmt(vfi, &vafp);

        strm->frame_buf = pj_pool_alloc(pool, vafp.framebytes);
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;

    return PJ_SUCCESS;

on_error:
    pj_pool_release(pool);
    return PJMEDIA_EVID_INVCAP;
}

/* API: Get stream info. */
static pj_status_t ffmpeg_stream_get_param(pjmedia_vid_dev_stream *s,
                                           pjmedia_vid_dev_param *pi)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t ffmpeg_stream_get_cap(pjmedia_vid_dev_stream *s,
                                         pjmedia_vid_dev_cap cap,
                                         void *pval)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;

    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(pval);

    return PJMEDIA_EVID_INVCAP;
}

/* API: set capability */
static pj_status_t ffmpeg_stream_set_cap(pjmedia_vid_dev_stream *s,
                                         pjmedia_vid_dev_cap cap,
                                         const void *pval)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;

    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(pval);

    return PJMEDIA_EVID_INVCAP;
}


/* API: Start stream. */
static pj_status_t ffmpeg_stream_start(pjmedia_vid_dev_stream *s)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;
    ffmpeg_dev_info *info;
    pj_status_t status;

    info = &strm->factory->dev_info[strm->param.cap_id];

    PJ_LOG(4, (THIS_FILE, "Starting ffmpeg capture stream"));

    status = ffmpeg_capture_open(&strm->ff_fmt_ctx, info->host_api,
                                 info->def_devname, &strm->param);
    if (status != PJ_SUCCESS) {
        /* must set ffmpeg states to NULL on any failure */
        strm->ff_fmt_ctx = NULL;
    }

    return status;
}


/* API: Get frame from stream */
static pj_status_t ffmpeg_stream_get_frame(pjmedia_vid_dev_stream *s,
                                           pjmedia_frame *frame)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;
    AVPacket p = {0};
    int err;

    err = av_read_frame(strm->ff_fmt_ctx, &p);
    if (err < 0) {
        print_ffmpeg_err(err);
        return PJ_EUNKNOWN;
    }

    pj_bzero(frame, sizeof(*frame));
    frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
    frame->buf = strm->frame_buf;
    frame->size = p.size;
    pj_memcpy(frame->buf, p.data, p.size);
    av_free_packet(&p);

    return PJ_SUCCESS;
}


/* API: Stop stream. */
static pj_status_t ffmpeg_stream_stop(pjmedia_vid_dev_stream *s)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;

    PJ_LOG(4, (THIS_FILE, "Stopping ffmpeg capture stream"));

    ffmpeg_capture_close(strm->ff_fmt_ctx);
    strm->ff_fmt_ctx = NULL;

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t ffmpeg_stream_destroy(pjmedia_vid_dev_stream *s)
{
    ffmpeg_stream *strm = (ffmpeg_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    ffmpeg_stream_stop(s);

    pj_pool_release(strm->pool);

    return PJ_SUCCESS;
}

#ifdef _MSC_VER
#   pragma comment( lib, "avdevice.lib")
#   pragma comment( lib, "avformat.lib")
#   pragma comment( lib, "avutil.lib")
#endif


#endif  /* PJMEDIA_VIDEO_DEV_HAS_FFMPEG */

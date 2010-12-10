/* $Id$ */
/*
 * Copyright (C) 2008-2010 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/converter.h>
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if PJMEDIA_VIDEO_DEV_HAS_SDL

#include <SDL.h>

#define THIS_FILE		"sdl_dev.c"
#define DEFAULT_CLOCK_RATE	90000
#define DEFAULT_WIDTH		640
#define DEFAULT_HEIGHT		480
#define DEFAULT_FPS		25


typedef struct sdl_fmt_info
{
    pjmedia_format_id   fmt_id;
    Uint32              sdl_format;
    Uint32              Rmask;
    Uint32              Gmask;
    Uint32              Bmask;
    Uint32              Amask;
} sdl_fmt_info;

static sdl_fmt_info sdl_fmts[] =
{
#if PJ_IS_BIG_ENDIAN
    {PJMEDIA_FORMAT_RGBA,  0, 0xFF000000, 0xFF0000, 0xFF00, 0xFF} ,
    {PJMEDIA_FORMAT_RGB24, 0, 0xFF0000, 0xFF00, 0xFF, 0} ,
    {PJMEDIA_FORMAT_BGRA,  0, 0xFF00, 0xFF0000, 0xFF000000, 0xFF} ,
#else
    {PJMEDIA_FORMAT_RGBA,  0, 0xFF, 0xFF00, 0xFF0000, 0xFF000000} ,
    {PJMEDIA_FORMAT_RGB24, 0, 0xFF, 0xFF00, 0xFF0000, 0} ,
    {PJMEDIA_FORMAT_BGRA,  0, 0xFF0000, 0xFF00, 0xFF, 0xFF000000} ,
#endif

    {PJMEDIA_FORMAT_DIB  , 0, 0xFF0000, 0xFF00, 0xFF, 0} ,

    {PJMEDIA_FORMAT_YUY2, SDL_YUY2_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_UYVY, SDL_UYVY_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_YVYU, SDL_YVYU_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I420, SDL_IYUV_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_YV12, SDL_YV12_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I420JPEG, SDL_IYUV_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I422JPEG, SDL_YV12_OVERLAY, 0, 0, 0, 0} ,
};

/* sdl_ device info */
struct sdl_dev_info
{
    pjmedia_vid_dev_info	 info;
};

/* sdl_ factory */
struct sdl_factory
{
    pjmedia_vid_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;

    unsigned			 dev_count;
    struct sdl_dev_info	        *dev_info;
};

/* Video stream. */
struct sdl_stream
{
    pjmedia_vid_stream	 base;		    /**< Base stream	       */
    pjmedia_vid_param	 param;		    /**< Settings	       */
    pj_pool_t           *pool;              /**< Memory pool.          */

    pjmedia_vid_cb       vid_cb;            /**< Stream callback.      */
    void                *user_data;         /**< Application data.     */

    pj_thread_t         *sdl_thread;        /**< SDL thread.           */
    pj_bool_t            is_quitting;
    pj_bool_t            is_running;
    pj_bool_t            render_exited;
    pj_status_t          status;

    SDL_Rect             rect;              /**< Display rectangle.    */
    SDL_Surface         *screen;            /**< Display screen.       */
    SDL_Surface         *surf;              /**< RGB surface.          */
    SDL_Overlay         *overlay;           /**< YUV overlay.          */

    /* For frame conversion */
    pjmedia_converter       *conv;
    pjmedia_conversion_param conv_param;
    pjmedia_frame            conv_buf;

    pjmedia_video_apply_fmt_param vafp;
};


/* Prototypes */
static pj_status_t sdl_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t sdl_factory_destroy(pjmedia_vid_dev_factory *f);
static unsigned    sdl_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t sdl_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					    unsigned index,
					    pjmedia_vid_dev_info *info);
static pj_status_t sdl_factory_default_param(pj_pool_t *pool,
                                             pjmedia_vid_dev_factory *f,
					     unsigned index,
					     pjmedia_vid_param *param);
static pj_status_t sdl_factory_create_stream(pjmedia_vid_dev_factory *f,
					     const pjmedia_vid_param *param,
					     const pjmedia_vid_cb *cb,
					     void *user_data,
					     pjmedia_vid_stream **p_vid_strm);

static pj_status_t sdl_stream_get_param(pjmedia_vid_stream *strm,
					pjmedia_vid_param *param);
static pj_status_t sdl_stream_get_cap(pjmedia_vid_stream *strm,
				      pjmedia_vid_dev_cap cap,
				      void *value);
static pj_status_t sdl_stream_set_cap(pjmedia_vid_stream *strm,
				      pjmedia_vid_dev_cap cap,
				      const void *value);
static pj_status_t sdl_stream_put_frame(pjmedia_vid_stream *strm,
                                        const pjmedia_frame *frame);
static pj_status_t sdl_stream_start(pjmedia_vid_stream *strm);
static pj_status_t sdl_stream_stop(pjmedia_vid_stream *strm);
static pj_status_t sdl_stream_destroy(pjmedia_vid_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &sdl_factory_init,
    &sdl_factory_destroy,
    &sdl_factory_get_dev_count,
    &sdl_factory_get_dev_info,
    &sdl_factory_default_param,
    &sdl_factory_create_stream
};

static pjmedia_vid_stream_op stream_op =
{
    &sdl_stream_get_param,
    &sdl_stream_get_cap,
    &sdl_stream_set_cap,
    &sdl_stream_start,
    NULL,
    &sdl_stream_put_frame,
    &sdl_stream_stop,
    &sdl_stream_destroy
};


/****************************************************************************
 * Factory operations
 */
/*
 * Init sdl_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_sdl_factory(pj_pool_factory *pf)
{
    struct sdl_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "sdl video", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct sdl_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t sdl_factory_init(pjmedia_vid_dev_factory *f)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    struct sdl_dev_info *ddi;
    unsigned i;

    sf->dev_count = 1;
    sf->dev_info = (struct sdl_dev_info*)
 		   pj_pool_calloc(sf->pool, sf->dev_count,
 				  sizeof(struct sdl_dev_info));

    ddi = &sf->dev_info[0];
    pj_bzero(ddi, sizeof(*ddi));
    strncpy(ddi->info.name, "SDL renderer", sizeof(ddi->info.name));
    ddi->info.name[sizeof(ddi->info.name)-1] = '\0';
    strncpy(ddi->info.driver, "SDL", sizeof(ddi->info.driver));
    ddi->info.driver[sizeof(ddi->info.driver)-1] = '\0';
    ddi->info.dir = PJMEDIA_DIR_RENDER;
    ddi->info.has_callback = PJ_FALSE;
    ddi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT |
                     PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;

    ddi->info.fmt_cnt = PJ_ARRAY_SIZE(sdl_fmts);
    ddi->info.fmt = (pjmedia_format*)
 		    pj_pool_calloc(sf->pool, ddi->info.fmt_cnt,
 				   sizeof(pjmedia_format));
    for (i = 0; i < ddi->info.fmt_cnt; i++) {
        pjmedia_format *fmt = &ddi->info.fmt[i];
        pjmedia_format_init_video(fmt, sdl_fmts[i].fmt_id,
				  DEFAULT_WIDTH, DEFAULT_HEIGHT,
				  DEFAULT_FPS, 1);
    }

    PJ_LOG(4, (THIS_FILE, "SDL initialized"));

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t sdl_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    pj_pool_t *pool = sf->pool;

    sf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned sdl_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    return sf->dev_count;
}

/* API: get device info */
static pj_status_t sdl_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					    unsigned index,
					    pjmedia_vid_dev_info *info)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;

    PJ_ASSERT_RETURN(index < sf->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &sf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t sdl_factory_default_param(pj_pool_t *pool,
                                             pjmedia_vid_dev_factory *f,
					     unsigned index,
					     pjmedia_vid_param *param)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    struct sdl_dev_info *di = &sf->dev_info[index];

    PJ_ASSERT_RETURN(index < sf->dev_count, PJMEDIA_EVID_INVDEV);
    
    PJ_UNUSED_ARG(pool);

    pj_bzero(param, sizeof(*param));
    if (di->info.dir & PJMEDIA_DIR_CAPTURE_RENDER) {
	param->dir = PJMEDIA_DIR_CAPTURE_RENDER;
	param->cap_id = index;
	param->rend_id = index;
    } else if (di->info.dir & PJMEDIA_DIR_CAPTURE) {
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->cap_id = index;
	param->rend_id = PJMEDIA_VID_INVALID_DEV;
    } else if (di->info.dir & PJMEDIA_DIR_RENDER) {
	param->dir = PJMEDIA_DIR_RENDER;
	param->rend_id = index;
	param->cap_id = PJMEDIA_VID_INVALID_DEV;
    } else {
	return PJMEDIA_EVID_INVDEV;
    }

    /* Set the device capabilities here */
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->fmt.type = PJMEDIA_TYPE_VIDEO;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    param->frame_rate.num = DEFAULT_FPS;
    param->frame_rate.denum = 1;
    pjmedia_format_init_video(&param->fmt, sdl_fmts[0].fmt_id,
			      DEFAULT_WIDTH, DEFAULT_HEIGHT,
			      DEFAULT_FPS, 1);

    return PJ_SUCCESS;
}

static sdl_fmt_info* get_sdl_format_info(pjmedia_format_id id)
{
    unsigned i;

    for (i = 0; i < sizeof(sdl_fmts)/sizeof(sdl_fmts[0]); i++) {
        if (sdl_fmts[i].fmt_id == id)
            return &sdl_fmts[i];
    }

    return NULL;
}

static int create_sdl_thread(void * data)
{
    struct sdl_stream *strm = (struct sdl_stream*)data;
    sdl_fmt_info *sdl_info = get_sdl_format_info(strm->param.fmt.id);
    const pjmedia_video_format_info *vfi;
    pjmedia_video_format_detail *vfd;

    vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                        strm->param.fmt.id);
    if (!vfi || !sdl_info) {
        strm->status = PJMEDIA_EVID_BADFORMAT;
        return strm->status;
    }

    strm->vafp.size = strm->param.fmt.det.vid.size;
    strm->vafp.buffer = NULL;
    if (vfi->apply_fmt(vfi, &strm->vafp) != PJ_SUCCESS) {
        strm->status = PJMEDIA_EVID_BADFORMAT;
        return strm->status;
    }

    /* Initialize the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO)) {
        PJ_LOG(4, (THIS_FILE, "Cannot initialize SDL"));
        strm->status = PJMEDIA_EVID_INIT;
        return strm->status;
    }

    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    strm->rect.x = strm->rect.y = 0;
    strm->rect.w = (Uint16)vfd->size.w;
    strm->rect.h = (Uint16)vfd->size.h;

    /* Initialize the display, requesting a software surface */
    strm->screen = SDL_SetVideoMode(strm->rect.w, strm->rect.h,
                                    0, SDL_RESIZABLE | SDL_SWSURFACE);
    if (strm->screen == NULL) {
        strm->status = PJMEDIA_EVID_SYSERR;
        return strm->status;
    }
    SDL_WM_SetCaption("pjmedia-SDL video", NULL);

    if (vfi->color_model == PJMEDIA_COLOR_MODEL_RGB) {
        strm->surf = SDL_CreateRGBSurface(SDL_SWSURFACE,
            strm->rect.w, strm->rect.h,
            vfi->bpp,
            sdl_info->Rmask,
            sdl_info->Gmask,
            sdl_info->Bmask,
            sdl_info->Amask);
        if (strm->surf == NULL) {
            strm->status = PJMEDIA_EVID_SYSERR;
            return strm->status;
        }
    } else if (vfi->color_model == PJMEDIA_COLOR_MODEL_YUV) {
        strm->overlay = SDL_CreateYUVOverlay(strm->rect.w, strm->rect.h,
            sdl_info->sdl_format,
            strm->screen);
        if (strm->overlay == NULL) {
            strm->status = PJMEDIA_EVID_SYSERR;
            return strm->status;
        }
    }

    while(!strm->is_quitting) {
        SDL_Event sevent;
        pjmedia_vid_event pevent;

        while (SDL_WaitEvent(&sevent)) {
            pj_bzero(&pevent, sizeof(pevent));

            switch(sevent.type) {
		case SDL_USEREVENT:
		    return 0;
                case SDL_MOUSEBUTTONDOWN:
                    pevent.event_type = PJMEDIA_EVENT_MOUSEBUTTONDOWN;
                    if (strm->vid_cb.on_event_cb)
                        if ((*strm->vid_cb.on_event_cb)(&strm->base,
                                                       strm->user_data,
                                                       &pevent) != PJ_SUCCESS)
                        {
                            /* Application wants us to ignore this event */
                            break;
                        }
                    if (strm->is_running)
                        pjmedia_vid_stream_stop(&strm->base);
                    else
                        pjmedia_vid_stream_start(&strm->base);
                    break;
                case SDL_VIDEORESIZE:
                    pevent.event_type = PJMEDIA_EVENT_WINDOW_RESIZE;
                    if (strm->vid_cb.on_event_cb)
                        if ((*strm->vid_cb.on_event_cb)(&strm->base,
                                                       strm->user_data,
                                                       &pevent) != PJ_SUCCESS)
                        {
                            /* Application wants us to ignore this event */
                            break;
                        }
                    /* TODO: move this to OUTPUT_RESIZE cap
                    strm->screen = SDL_SetVideoMode(sevent.resize.w,
                                                    sevent.resize.h,
                                                    0, SDL_RESIZABLE |
                                                    SDL_SWSURFACE);
                    */                  
                    break;
                case SDL_QUIT:
                    pevent.event_type = PJMEDIA_EVENT_WINDOW_CLOSE;
                    /**
                     * To process PJMEDIA_EVENT_WINDOW_CLOSE event,
                     * application should do this in the on_event_cb callback:
                     * 1. stop further calls to #pjmedia_vid_stream_put_frame()
                     * 2. return PJ_SUCCESS
                     * Upon returning from the callback, SDL will destroy its
                     * own stream.
                     *
                     * Returning non-PJ_SUCCESS will cause SDL to ignore
                     * the event
                     */
                    if (strm->vid_cb.on_event_cb) {
                        strm->is_quitting = PJ_TRUE;
                        if ((*strm->vid_cb.on_event_cb)(&strm->base,
                                                       strm->user_data,
                                                       &pevent) != PJ_SUCCESS)
                        {
                            /* Application wants us to ignore this event */
                            strm->is_quitting = PJ_FALSE;
                            break;
                        }

                        /* Destroy the stream */
                        sdl_stream_destroy(&strm->base);
                        return 0;
                    }

                    /**
                     * Default event-handler when there is no user-specified
                     * callback: close the renderer window. We cannot destroy
                     * the stream here since there is no callback to notify
                     * the application.
                     */
                    sdl_stream_stop(&strm->base);
                    SDL_Quit();
                    strm->screen = NULL;
                    return 0;
                default:
                    break;
            }
        }

    }

    return 0;
}

/* API: create stream */
static pj_status_t sdl_factory_create_stream(pjmedia_vid_dev_factory *f,
					     const pjmedia_vid_param *param,
					     const pjmedia_vid_cb *cb,
					     void *user_data,
					     pjmedia_vid_stream **p_vid_strm)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    pj_pool_t *pool;
    struct sdl_stream *strm;
    pj_status_t status;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(sf->pf, "sdl-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct sdl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;

    /* Create capture stream here */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
    }

    /* Create render stream here */
    if (param->dir & PJMEDIA_DIR_RENDER) {
        strm->status = PJ_SUCCESS;
        status = pj_thread_create(pool, "sdl_thread", create_sdl_thread,
                                  strm, 0, 0, &strm->sdl_thread);
        if (status != PJ_SUCCESS) {
            goto on_error;
        }
        while (strm->status == PJ_SUCCESS && !strm->surf && !strm->overlay)
            pj_thread_sleep(10);
        if ((status = strm->status) != PJ_SUCCESS) {
            goto on_error;
        }

        pjmedia_format_copy(&strm->conv_param.src, &param->fmt);
        pjmedia_format_copy(&strm->conv_param.dst, &param->fmt);

        status = pjmedia_converter_create(NULL, pool, &strm->conv_param,
                                          &strm->conv);
    }

    /* Apply the remaining settings */
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
	sdl_stream_set_cap(&strm->base,
		            PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                            &param->window);
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;

    return PJ_SUCCESS;

on_error:
    sdl_stream_destroy(&strm->base);
    return status;
}

/* API: Get stream info. */
static pj_status_t sdl_stream_get_param(pjmedia_vid_stream *s,
					pjmedia_vid_param *pi)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

/*    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                            &pi->fmt.info_size) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
*/
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t sdl_stream_get_cap(pjmedia_vid_stream *s,
				      pjmedia_vid_dev_cap cap,
				      void *pval)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW)
    {
	return PJ_SUCCESS;
    } else {
	return PJMEDIA_EVID_INVCAP;
    }
}

/* API: set capability */
static pj_status_t sdl_stream_set_cap(pjmedia_vid_stream *s,
				      pjmedia_vid_dev_cap cap,
				      const void *pval)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW)
    {
	return PJ_SUCCESS;
    }

    return PJMEDIA_EVID_INVCAP;
}

/* API: Put frame from stream */
static pj_status_t sdl_stream_put_frame(pjmedia_vid_stream *strm,
                                        const pjmedia_frame *frame)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;

    if (!stream->is_running) {
        stream->render_exited = PJ_TRUE;
        return PJ_SUCCESS;
    }

    if (stream->surf) {
        if (SDL_MUSTLOCK(stream->surf)) {
            if (SDL_LockSurface(stream->surf) < 0) {
                PJ_LOG(3, (THIS_FILE, "Unable to lock SDL surface"));
                return PJMEDIA_EVID_NOTREADY;
            }
        }

        pj_memcpy(stream->surf->pixels, frame->buf, frame->size);

        if (SDL_MUSTLOCK(stream->surf)) {
            SDL_UnlockSurface(stream->surf);
        }
        SDL_BlitSurface(stream->surf, NULL, stream->screen, NULL);
        SDL_UpdateRect(stream->screen, 0, 0, 0, 0);
    } else if (stream->overlay) {
        int i, sz, offset;

        if (SDL_LockYUVOverlay(stream->overlay) < 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to lock SDL overlay"));
            return PJMEDIA_EVID_NOTREADY;
        }

        for (i = 0, offset = 0; i < stream->overlay->planes; i++) {
            sz = stream->vafp.plane_bytes[i];
            pj_memcpy(stream->overlay->pixels[i],
                      (char *)frame->buf + offset, sz);
            offset += sz;
        }

        SDL_UnlockYUVOverlay(stream->overlay);
        SDL_DisplayYUVOverlay(stream->overlay, &stream->rect);
    }

    return PJ_SUCCESS;
}

/* API: Start stream. */
static pj_status_t sdl_stream_start(pjmedia_vid_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;

    PJ_LOG(4, (THIS_FILE, "Starting sdl video stream"));

    stream->is_running = PJ_TRUE;
    stream->render_exited = PJ_FALSE;

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t sdl_stream_stop(pjmedia_vid_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;
    unsigned i;

    PJ_LOG(4, (THIS_FILE, "Stopping sdl video stream"));

    /* Wait for renderer put_frame() to finish */
    stream->is_running = PJ_FALSE;
    for (i=0; !stream->render_exited && i<100; ++i)
	pj_thread_sleep(10);

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t sdl_stream_destroy(pjmedia_vid_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;
    SDL_Event sevent;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    if (!stream->is_quitting) {
        sevent.type = SDL_USEREVENT;
        SDL_PushEvent(&sevent);
        pj_thread_join(stream->sdl_thread);
    }

    sdl_stream_stop(strm);

    if (stream->surf) {
        SDL_FreeSurface(stream->surf);
        stream->surf = NULL;
    }

    if (stream->overlay) {
        SDL_FreeYUVOverlay(stream->overlay);
        stream->overlay = NULL;
    }

    SDL_Quit();
    stream->screen = NULL;

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_SDL */

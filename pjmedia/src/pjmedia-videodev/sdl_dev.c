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

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
#   include <Foundation/Foundation.h>
#endif

#include <SDL.h>
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#   include "SDL_opengl.h"
#   define OPENGL_DEV_IDX 1
#else
#   define OPENGL_DEV_IDX -999
#endif

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
    {PJMEDIA_FORMAT_RGBA,  SDL_PIXELFORMAT_RGBA8888,
     0xFF000000, 0xFF0000, 0xFF00, 0xFF} ,
    {PJMEDIA_FORMAT_RGB24, SDL_PIXELFORMAT_RGB24,
     0xFF0000, 0xFF00, 0xFF, 0} ,
    {PJMEDIA_FORMAT_BGRA,  SDL_PIXELFORMAT_BGRA8888,
     0xFF00, 0xFF0000, 0xFF000000, 0xFF} ,
#else
    {PJMEDIA_FORMAT_RGBA,  SDL_PIXELFORMAT_ABGR8888,
     0xFF, 0xFF00, 0xFF0000, 0xFF000000} ,
    {PJMEDIA_FORMAT_RGB24, SDL_PIXELFORMAT_BGR24,
     0xFF, 0xFF00, 0xFF0000, 0} ,
    {PJMEDIA_FORMAT_BGRA,  SDL_PIXELFORMAT_ARGB8888,
     0xFF0000, 0xFF00, 0xFF, 0xFF000000} ,
#endif

    {PJMEDIA_FORMAT_DIB  , SDL_PIXELFORMAT_RGB24,
     0xFF0000, 0xFF00, 0xFF, 0} ,

    {PJMEDIA_FORMAT_YUY2, SDL_YUY2_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_UYVY, SDL_UYVY_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_YVYU, SDL_YVYU_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I420, SDL_IYUV_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_YV12, SDL_YV12_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I420JPEG, SDL_IYUV_OVERLAY, 0, 0, 0, 0} ,
    {PJMEDIA_FORMAT_I422JPEG, SDL_YV12_OVERLAY, 0, 0, 0, 0} ,
};

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
@interface SDLDelegate: NSObject
{
    @public
    struct sdl_stream	    *strm;
}

- (void)sdl_init;
- (void)sdl_quit;
- (void)detect_new_fmt;
- (int)sdl_create;
- (void)sdl_destroy;
- (int)handle_event;
- (pj_status_t)put_frame;
@end
#endif

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
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    NSAutoreleasePool		*apool;
    SDLDelegate			*delegate;
#endif
};

/* Video stream. */
struct sdl_stream
{
    pjmedia_vid_dev_stream	 base;		    /**< Base stream	    */
    pjmedia_vid_param		 param;		    /**< Settings	    */
    pj_pool_t			*pool;              /**< Memory pool.       */

    pjmedia_vid_cb		 vid_cb;            /**< Stream callback.   */
    void			*user_data;         /**< Application data.  */

    pj_thread_t			*sdl_thread;        /**< SDL thread.        */
    pj_bool_t			 is_quitting;
    pj_bool_t			 is_running;
    pj_bool_t			 render_exited;
    pj_status_t			 status;
    pjmedia_format              *new_fmt;

#if SDL_VERSION_ATLEAST(1,3,0)
    SDL_Window                  *window;            /**< Display window.    */
    SDL_Renderer                *renderer;          /**< Display renderer.  */
    SDL_Texture                 *scr_tex;           /**< Screen texture.    */
    int                          pitch;             /**< Pitch value.       */
#endif
    SDL_Rect			 rect;              /**< Display rectangle. */
    SDL_Surface			*screen;            /**< Display screen.    */
    SDL_Surface			*surf;              /**< RGB surface.       */
    SDL_Overlay			*overlay;           /**< YUV overlay.       */
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#if SDL_VERSION_ATLEAST(1,3,0)
    SDL_GLContext               *gl_context;
#endif
    GLuint			 texture;
    void			*tex_buf;
    pj_size_t			 tex_buf_size;
#endif
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    NSAutoreleasePool		*apool;
    SDLDelegate			*delegate;
    const pjmedia_frame		*frame;
#endif

    /* For frame conversion */
    pjmedia_converter		*conv;
    pjmedia_conversion_param	 conv_param;
    pjmedia_frame		 conv_buf;

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
static pj_status_t sdl_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_param *param,
					const pjmedia_vid_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t sdl_stream_get_param(pjmedia_vid_dev_stream *strm,
					pjmedia_vid_param *param);
static pj_status_t sdl_stream_get_cap(pjmedia_vid_dev_stream *strm,
				      pjmedia_vid_dev_cap cap,
				      void *value);
static pj_status_t sdl_stream_set_cap(pjmedia_vid_dev_stream *strm,
				      pjmedia_vid_dev_cap cap,
				      const void *value);
static pj_status_t sdl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                        const pjmedia_frame *frame);
static pj_status_t sdl_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t sdl_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t sdl_stream_destroy(pjmedia_vid_dev_stream *strm);

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
static void draw_gl(struct sdl_stream *stream, void *tex_buf);
#endif

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

static pjmedia_vid_dev_stream_op stream_op =
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
    unsigned i, j;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    sf->apool = [[NSAutoreleasePool alloc] init];
    sf->delegate = [[SDLDelegate alloc] init];
    [sf->delegate performSelectorOnMainThread:@selector(sdl_init) 
	          withObject:nil waitUntilDone:YES];
#else
    /* Initialize the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO)) {
        PJ_LOG(4, (THIS_FILE, "Cannot initialize SDL"));
        return PJMEDIA_EVID_INIT;
    }
#endif

    sf->dev_count = 1;
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    sf->dev_count++;
#endif
    sf->dev_info = (struct sdl_dev_info*)
		   pj_pool_calloc(sf->pool, sf->dev_count,
				  sizeof(struct sdl_dev_info));

    ddi = &sf->dev_info[0];
    pj_bzero(ddi, sizeof(*ddi));
    strncpy(ddi->info.name, "SDL renderer", sizeof(ddi->info.name));
    ddi->info.name[sizeof(ddi->info.name)-1] = '\0';
    ddi->info.fmt_cnt = PJ_ARRAY_SIZE(sdl_fmts);

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    ddi = &sf->dev_info[OPENGL_DEV_IDX];
    pj_bzero(ddi, sizeof(*ddi));
    strncpy(ddi->info.name, "SDL openGL renderer", sizeof(ddi->info.name));
    ddi->info.name[sizeof(ddi->info.name)-1] = '\0';
    ddi->info.fmt_cnt = 1;
#endif

    for (i = 0; i < sf->dev_count; i++) {
        ddi = &sf->dev_info[i];
        strncpy(ddi->info.driver, "SDL", sizeof(ddi->info.driver));
        ddi->info.driver[sizeof(ddi->info.driver)-1] = '\0';
        ddi->info.dir = PJMEDIA_DIR_RENDER;
        ddi->info.has_callback = PJ_FALSE;
        ddi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT |
                         PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;
#if SDL_VERSION_ATLEAST(1,3,0)
        ddi->info.caps |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
#endif

        ddi->info.fmt = (pjmedia_format*)
                        pj_pool_calloc(sf->pool, ddi->info.fmt_cnt,
                                       sizeof(pjmedia_format));
        for (j = 0; j < ddi->info.fmt_cnt; j++) {
            pjmedia_format *fmt = &ddi->info.fmt[j];
            pjmedia_format_init_video(fmt, sdl_fmts[j].fmt_id,
                                      DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                      DEFAULT_FPS, 1);
        }
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

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    [sf->delegate performSelectorOnMainThread:@selector(sdl_quit) 
                  withObject:nil waitUntilDone:YES];
    [sf->delegate release];
    [sf->apool release];
#else
    SDL_Quit();
#endif

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
    if (di->info.dir == PJMEDIA_DIR_CAPTURE_RENDER) {
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

static void destroy_sdl(struct sdl_stream *strm, pj_bool_t destroy_win)
{
    if (strm->surf) {
	SDL_FreeSurface(strm->surf);
	strm->surf = NULL;
    }
    if (strm->overlay) {
	SDL_FreeYUVOverlay(strm->overlay);
	strm->overlay = NULL;
    }
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->texture) {
	glDeleteTextures(1, &strm->texture);
	strm->texture = 0;
    }
#endif
#if SDL_VERSION_ATLEAST(1,3,0)
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->gl_context) {
        SDL_GL_DeleteContext(strm->gl_context);
        strm->gl_context = NULL;
    }
#endif
    if (strm->scr_tex) {
        SDL_DestroyTexture(strm->scr_tex);
        strm->scr_tex = NULL;
    }
    if (strm->renderer) {
        SDL_DestroyRenderer(strm->renderer);
        strm->renderer = NULL;
    }
#ifndef __IPHONEOS__
    if (destroy_win) {
        if (strm->window &&
            !(strm->param.flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW))
        {
            SDL_DestroyWindow(strm->window);
        }
        strm->window = NULL;
    }
#endif
#endif
}

static pj_status_t init_sdl(struct sdl_stream *strm, pjmedia_format *fmt)
{
    sdl_fmt_info *sdl_info = get_sdl_format_info(fmt->id);
    const pjmedia_video_format_info *vfi;
    pjmedia_video_format_detail *vfd;

    vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                        fmt->id);
    if (!vfi || !sdl_info)
        return PJMEDIA_EVID_BADFORMAT;

    strm->vafp.size = fmt->det.vid.size;
    strm->vafp.buffer = NULL;
    if (vfi->apply_fmt(vfi, &strm->vafp) != PJ_SUCCESS)
        return PJMEDIA_EVID_BADFORMAT;

    vfd = pjmedia_format_get_video_format_detail(fmt, PJ_TRUE);
    strm->rect.x = strm->rect.y = 0;
    strm->rect.w = (Uint16)vfd->size.w;
    strm->rect.h = (Uint16)vfd->size.h;

    destroy_sdl(strm, PJ_FALSE);

#if SDL_VERSION_ATLEAST(1,3,0)
    if (!strm->window) {
        Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

        if (strm->param.rend_id == OPENGL_DEV_IDX)
            flags |= SDL_WINDOW_OPENGL;

        if (strm->param.flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
            /* Use the window supplied by the application. */
            strm->window = SDL_CreateWindowFrom(strm->param.window);
        } else {
            /* Create the window where we will draw. */
            strm->window = SDL_CreateWindow("pjmedia-SDL video",
                                            SDL_WINDOWPOS_CENTERED,
                                            SDL_WINDOWPOS_CENTERED,
                                            strm->rect.w, strm->rect.h,
                                            flags);
        }
        if (!strm->window)
            return PJMEDIA_EVID_SYSERR;
    }

    SDL_SetWindowSize(strm->window, strm->rect.w, strm->rect.h);

    /**
      * We must call SDL_CreateRenderer in order for draw calls to
      * affect this window.
      */
    strm->renderer = SDL_CreateRenderer(strm->window, -1, 0);
    if (!strm->renderer)
        return PJMEDIA_EVID_SYSERR;

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
        strm->gl_context = SDL_GL_CreateContext(strm->window);
        if (!strm->gl_context)
            return PJMEDIA_EVID_SYSERR;
        SDL_GL_MakeCurrent(strm->window, strm->gl_context);
    }
#endif

    strm->screen = SDL_GetWindowSurface(strm->window);

#else

    /* Initialize the display */
    strm->screen = SDL_SetVideoMode(strm->rect.w, strm->rect.h, 0, (
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
                                    strm->param.rend_id == OPENGL_DEV_IDX?
				    SDL_OPENGL | SDL_RESIZABLE:
#endif
				    SDL_RESIZABLE | SDL_SWSURFACE));
    if (strm->screen == NULL)
        return PJMEDIA_EVID_SYSERR;

    SDL_WM_SetCaption("pjmedia-SDL video", NULL);

#endif

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
	/* Init some OpenGL settings */
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	
	/* Init the viewport */
	glViewport(0, 0, strm->rect.w, strm->rect.h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	glOrtho(0.0, (GLdouble)strm->rect.w, (GLdouble)strm->rect.h,
		0.0, 0.0, 1.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	/* Create a texture */
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glGenTextures(1, &strm->texture);

        if (!strm->texture)
            return PJMEDIA_EVID_SYSERR;

#if defined(PJ_WIN32) && PJ_WIN32 != 0
	/**
	 * On Win32 platform, the OpenGL drawing must be in the same
	 * thread that calls SDL_SetVideoMode(), hence we need a buffer
	 * for the frame from sdl_stream_put_frame()
	 */
	if (strm->vafp.framebytes > strm->tex_buf_size) {
	    strm->tex_buf_size = strm->vafp.framebytes;
	    strm->tex_buf = pj_pool_alloc(strm->pool, strm->vafp.framebytes);
	}
#endif
    } else
#endif
#if SDL_VERSION_ATLEAST(1,3,0)
    {    
        strm->scr_tex = SDL_CreateTexture(strm->renderer, sdl_info->sdl_format,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          strm->rect.w, strm->rect.h);
        if (strm->scr_tex == NULL)
            return PJMEDIA_EVID_SYSERR;
    
        strm->pitch = strm->rect.w * SDL_BYTESPERPIXEL(sdl_info->sdl_format);
    }
#else
    if (vfi->color_model == PJMEDIA_COLOR_MODEL_RGB) {
        strm->surf = SDL_CreateRGBSurface(SDL_SWSURFACE,
					  strm->rect.w, strm->rect.h,
					  vfi->bpp,
					  sdl_info->Rmask,
					  sdl_info->Gmask,
					  sdl_info->Bmask,
					  sdl_info->Amask);
        if (strm->surf == NULL)
            return PJMEDIA_EVID_SYSERR;
    } else if (vfi->color_model == PJMEDIA_COLOR_MODEL_YUV) {
        strm->overlay = SDL_CreateYUVOverlay(strm->rect.w, strm->rect.h,
					     sdl_info->sdl_format,
					     strm->screen);
        if (strm->overlay == NULL)
            return PJMEDIA_EVID_SYSERR;
    }
#endif

    return PJ_SUCCESS;
}

static void detect_fmt_change(struct sdl_stream *strm)
{
    if (strm->new_fmt) {
        /* Stop the stream */
        sdl_stream_stop((pjmedia_vid_dev_stream *)strm);
        
        /* Re-initialize SDL */
        strm->status = init_sdl(strm, strm->new_fmt);
        
        if (strm->status == PJ_SUCCESS) {
            pjmedia_format_copy(&strm->param.fmt, strm->new_fmt);
            /* Restart the stream */
            sdl_stream_start((pjmedia_vid_dev_stream *)strm);
        }
        strm->new_fmt = NULL;
    }
}

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
@implementation SDLDelegate
- (void)sdl_init
{
    if (SDL_Init(SDL_INIT_VIDEO)) {
        PJ_LOG(4, (THIS_FILE, "Cannot initialize SDL"));
    }
}

- (void)sdl_quit
{
    SDL_Quit();
}

- (void)detect_new_fmt
{
    detect_fmt_change(strm);
}

- (int)sdl_create
{
#else
static int sdlthread(void * data)
{
    struct sdl_stream *strm = (struct sdl_stream*)data;
#endif

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    }
#endif

    strm->status = init_sdl(strm, &strm->param.fmt);
    if (strm->status != PJ_SUCCESS)
	goto on_return;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
on_return:
    if (strm->status != PJ_SUCCESS) {
	destroy_sdl(strm, PJ_TRUE);
	strm->screen = NULL;
    }
    
    return strm->status;
}

- (void)sdl_destroy
{
    destroy_sdl(strm, PJ_TRUE);
}    

- (int)handle_event
{
    const pjmedia_video_format_info *vfi;
    pjmedia_video_format_detail *vfd;
    
    vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
					strm->param.fmt.id);
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
#else
    while(!strm->is_quitting) 
#endif
    {
        SDL_Event sevent;
        pjmedia_vid_event pevent;

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#if defined(PJ_WIN32) && PJ_WIN32 != 0	
	if (strm->param.rend_id == OPENGL_DEV_IDX) {
	    draw_gl(strm, strm->tex_buf);
	}
#endif
#endif

        detect_fmt_change(strm);
        
        /**
         * The event polling must be placed in the same thread that
         * call SDL_SetVideoMode(). Please consult the official doc of
         * SDL_PumpEvents().
         */
        while (SDL_PollEvent(&sevent)) {
            pj_bzero(&pevent, sizeof(pevent));

            switch(sevent.type) {
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
                        pjmedia_vid_dev_stream_stop(&strm->base);
                    else
                        pjmedia_vid_dev_stream_start(&strm->base);
                    break;

                case SDL_VIDEORESIZE:
                    pevent.event_type = PJMEDIA_EVENT_WINDOW_RESIZE;
                    pevent.event_desc.resize.new_size.w = sevent.resize.w;
                    pevent.event_desc.resize.new_size.h = sevent.resize.h;
                    if (strm->vid_cb.on_event_cb) {
                        /** 
                         * To process PJMEDIA_EVENT_WINDOW_RESIZE event,
                         * application should do this in the on_event_cb
                         * callback:
                         * 1. change the input frame size given to SDL
                         *    to the new size.
                         * 2. call pjmedia_vid_dev_stream_set_cap()
                         *    using PJMEDIA_VID_DEV_CAP_FORMAT capability
                         *    and the new format size
                         */
                        (*strm->vid_cb.on_event_cb)(&strm->base,
                                                    strm->user_data,
                                                    &pevent);
                    }
                    break;

                case SDL_QUIT:
                    pevent.event_type = PJMEDIA_EVENT_WINDOW_CLOSE;
                    /**
                     * To process PJMEDIA_EVENT_WINDOW_CLOSE event,
                     * application should do this in the on_event_cb callback:
                     * 1. stop further calls to 
		     *    #pjmedia_vid_dev_stream_put_frame()
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
                        goto on_return;
                    }

                    /**
                     * Default event-handler when there is no user-specified
                     * callback: close the renderer window. We cannot destroy
                     * the stream here since there is no callback to notify
                     * the application.
                     */
                    sdl_stream_stop(&strm->base);
                    goto on_return;

                default:
                    break;
            }
	}
    }

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    return 0;
#endif
on_return:
    destroy_sdl(strm, PJ_TRUE);
    strm->screen = NULL;

    return strm->status;
}

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
static void draw_gl(struct sdl_stream *stream, void *tex_buf)
{
    if (stream->texture) {
	glBindTexture(GL_TEXTURE_2D, stream->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     stream->rect.w, stream->rect.h, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, tex_buf);
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 0); glVertex2i(0, 0);
	glTexCoord2f(1, 0); glVertex2i(stream->rect.w, 0);
	glTexCoord2f(0, 1); glVertex2i(0, stream->rect.h);
	glTexCoord2f(1, 1); glVertex2i(stream->rect.w, stream->rect.h);
	glEnd();
#if SDL_VERSION_ATLEAST(1,3,0)
        SDL_GL_SwapWindow(stream->window);
#else
	SDL_GL_SwapBuffers();
#endif
    }
}
#endif

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
- (pj_status_t)put_frame
{
    const pjmedia_frame *frame = strm->frame;
#else    
/* API: Put frame from stream */
static pj_status_t sdl_stream_put_frame(pjmedia_vid_dev_stream *strm,
					const pjmedia_frame *frame)
{
#endif
    struct sdl_stream *stream = (struct sdl_stream*)strm;
    pj_status_t status = PJ_SUCCESS;

    if (!stream->is_running) {
	stream->render_exited = PJ_TRUE;
	goto on_return;
    }

    if (frame->size==0 || frame->buf==NULL ||
	frame->size < stream->vafp.framebytes)
	goto on_return;

    if (stream->surf) {
	if (SDL_MUSTLOCK(stream->surf)) {
	    if (SDL_LockSurface(stream->surf) < 0) {
		PJ_LOG(3, (THIS_FILE, "Unable to lock SDL surface"));
		status = PJMEDIA_EVID_NOTREADY;
		goto on_return;
	    }
	}
	
	pj_memcpy(stream->surf->pixels, frame->buf,
		  stream->vafp.framebytes);
	
	if (SDL_MUSTLOCK(stream->surf)) {
	    SDL_UnlockSurface(stream->surf);
	}
	SDL_BlitSurface(stream->surf, NULL, stream->screen, NULL);
#if SDL_VERSION_ATLEAST(1,3,0)
        SDL_UpdateWindowSurface(stream->window);
#else
        SDL_UpdateRect(stream->screen, 0, 0, 0, 0);
#endif
    } else if (stream->overlay) {
	int i, sz, offset;
	
	if (SDL_LockYUVOverlay(stream->overlay) < 0) {
	    PJ_LOG(3, (THIS_FILE, "Unable to lock SDL overlay"));
	    status = PJMEDIA_EVID_NOTREADY;
	    goto on_return;
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
#if SDL_VERSION_ATLEAST(1,3,0)
    else if (stream->scr_tex) {
        SDL_UpdateTexture(stream->scr_tex, NULL, frame->buf, stream->pitch);
        SDL_RenderClear(stream->renderer);
        SDL_RenderCopy(stream->renderer, stream->scr_tex, NULL, NULL);
        SDL_RenderPresent(stream->renderer);
    }
#endif
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    else if (stream->param.rend_id == OPENGL_DEV_IDX) {
#if defined(PJ_WIN32) && PJ_WIN32 != 0
	pj_memcpy(stream->tex_buf, frame->buf, stream->vafp.framebytes);
#else
	draw_gl(stream, frame->buf);
#endif
    }
#endif

on_return:
    return status;
}
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
@end

static pj_status_t sdl_stream_put_frame(pjmedia_vid_dev_stream *strm,
					const pjmedia_frame *frame)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;
    stream->frame = frame;    
    [stream->delegate performSelectorOnMainThread:@selector(put_frame) 
	              withObject:nil waitUntilDone:YES];

    return PJ_SUCCESS;
}
    
static int sdlthread(void * data)
{
    struct sdl_stream *strm = (struct sdl_stream*)data;
    
    while(!strm->is_quitting) {
	[strm->delegate performSelectorOnMainThread:@selector(handle_event)
			withObject:nil waitUntilDone:YES];
    }
    
    return 0;
}
    
#endif

/* API: create stream */
static pj_status_t sdl_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_param *param,
					const pjmedia_vid_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm)
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
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
	strm->apool = [[NSAutoreleasePool alloc] init];
	strm->delegate = [[SDLDelegate alloc]init];
	strm->delegate->strm = strm;
	/* On Darwin OS, we need to call SDL functions in the main thread */
	[strm->delegate performSelectorOnMainThread:@selector(sdl_create)
			withObject:nil waitUntilDone:YES];
	if ((status = strm->status) != PJ_SUCCESS) {
	    goto on_error;
	}
#endif
	status = pj_thread_create(pool, "sdl_thread", sdlthread,
				  strm, 0, 0, &strm->sdl_thread);

	if (status != PJ_SUCCESS) {
	    goto on_error;
	}

	while(strm->status == PJ_SUCCESS && !strm->surf && !strm->overlay
#if SDL_VERSION_ATLEAST(1,3,0)
              && !strm->scr_tex
#endif
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
	      && !strm->texture
#endif
	      )
	{
	    pj_thread_sleep(10);
	}
	if ((status = strm->status) != PJ_SUCCESS) {
	    goto on_error;
	}

	pjmedia_format_copy(&strm->conv_param.src, &param->fmt);
	pjmedia_format_copy(&strm->conv_param.dst, &param->fmt);
	/*
	status = pjmedia_converter_create(NULL, pool, &strm->conv_param,
					  &strm->conv);
	 */
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
static pj_status_t sdl_stream_get_param(pjmedia_vid_dev_stream *s,
					pjmedia_vid_param *pi)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    /*
    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
			   &pi->fmt.info_size) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
    */
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t sdl_stream_get_cap(pjmedia_vid_dev_stream *s,
				      pjmedia_vid_dev_cap cap,
				      void *pval)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW)
    {
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_FORMAT) {
	return PJ_SUCCESS;
    } else {
	return PJMEDIA_EVID_INVCAP;
    }
}

/* API: set capability */
static pj_status_t sdl_stream_set_cap(pjmedia_vid_dev_stream *s,
				      pjmedia_vid_dev_cap cap,
				      const void *pval)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_FORMAT) {
        strm->new_fmt = (pjmedia_format *)pval;
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
        [strm->delegate performSelectorOnMainThread:@selector(detect_new_fmt)
                        withObject:nil waitUntilDone:YES];
#endif
	while (strm->new_fmt)
	    pj_thread_sleep(10);
	
	if (strm->status != PJ_SUCCESS) {
	    pj_status_t status = strm->status;
	    
	    /**
	     * Failed to change the output format. Try to revert
	     * to its original format.
	     */
            strm->new_fmt = &strm->param.fmt;
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
            [strm->delegate performSelectorOnMainThread:@selector(detect_new_fmt)
                            withObject:nil waitUntilDone:YES];
#endif
	    while (strm->new_fmt)
		pj_thread_sleep(10);
	    
	    if (strm->status != PJ_SUCCESS) {
		/**
		 * This means that we failed to revert to our
		 * original state!
		 */
		status = PJMEDIA_EVID_ERR;
	    }
	    
	    strm->status = status;
	}
	
	return strm->status;
    }

    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t sdl_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;

    PJ_LOG(4, (THIS_FILE, "Starting sdl video stream"));

    stream->is_running = PJ_TRUE;
    stream->render_exited = PJ_FALSE;

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t sdl_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;
    unsigned i;

    PJ_LOG(4, (THIS_FILE, "Stopping sdl video stream"));

    /* Wait for renderer put_frame() to finish */
    stream->is_running = PJ_FALSE;
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    if (![NSThread isMainThread])
#endif
    for (i=0; !stream->render_exited && i<50; ++i)
	pj_thread_sleep(10);

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t sdl_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    sdl_stream_stop(strm);

    if (!stream->is_quitting) {
        stream->is_quitting = PJ_TRUE;
	if (stream->sdl_thread)
	    pj_thread_join(stream->sdl_thread);
    }

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    if (stream->delegate) {
        [stream->delegate performSelectorOnMainThread:@selector(sdl_destroy)
                          withObject:nil waitUntilDone:YES];
	[stream->delegate release];
        stream->delegate = NULL;
    }
    if (stream->apool) {
	[stream->apool release];
        stream->apool = NULL;
    }
#endif
    pj_pool_release(stream->pool);
    

    return PJ_SUCCESS;
}

#ifdef _MSC_VER
#   pragma comment( lib, "sdl.lib")
#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#	pragma comment(lib, "OpenGL32.lib")
#   endif
#endif

#endif	/* PJMEDIA_VIDEO_DEV_HAS_SDL */

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
#include <pjmedia/converter.h>
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if defined(PJMEDIA_VIDEO_DEV_HAS_SDL) && PJMEDIA_VIDEO_DEV_HAS_SDL != 0

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
#   include "TargetConditionals.h"
#   include <Foundation/Foundation.h>
#   define SDL_USE_ONE_THREAD_PER_DISPLAY 1
#elif defined(PJ_WIN32) && PJ_WIN32 != 0 
#   define SDL_USE_ONE_THREAD_PER_DISPLAY 1
#else
#   define SDL_USE_ONE_THREAD_PER_DISPLAY 0
#endif

#include <SDL.h>
#include <SDL_syswm.h>
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#   include "SDL_opengl.h"
#   define OPENGL_DEV_IDX 1
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

#define THIS_FILE		"sdl_dev.c"
#define DEFAULT_CLOCK_RATE	90000
#define DEFAULT_WIDTH		640
#define DEFAULT_HEIGHT		480
#define DEFAULT_FPS		25

#if !(SDL_VERSION_ATLEAST(1,3,0))
#   define SDL_PIXELFORMAT_RGBA8888 0
#   define SDL_PIXELFORMAT_RGB24    0
#   define SDL_PIXELFORMAT_BGRA8888 0
#   define SDL_PIXELFORMAT_ABGR8888 0
#   define SDL_PIXELFORMAT_BGR24    0
#   define SDL_PIXELFORMAT_ARGB8888 0
#   define SDL_PIXELFORMAT_RGB24    0
#endif /* !(SDL_VERSION_ATLEAST(1,3,0)) */

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
    {PJMEDIA_FORMAT_RGBA,  (Uint32)SDL_PIXELFORMAT_RGBA8888,
     0xFF000000, 0xFF0000, 0xFF00, 0xFF} ,
    {PJMEDIA_FORMAT_RGB24, (Uint32)SDL_PIXELFORMAT_RGB24,
     0xFF0000, 0xFF00, 0xFF, 0} ,
    {PJMEDIA_FORMAT_BGRA,  (Uint32)SDL_PIXELFORMAT_BGRA8888,
     0xFF00, 0xFF0000, 0xFF000000, 0xFF} ,
#else /* PJ_IS_BIG_ENDIAN */
    {PJMEDIA_FORMAT_RGBA,  (Uint32)SDL_PIXELFORMAT_ABGR8888,
     0xFF, 0xFF00, 0xFF0000, 0xFF000000} ,
    {PJMEDIA_FORMAT_RGB24, (Uint32)SDL_PIXELFORMAT_BGR24,
     0xFF, 0xFF00, 0xFF0000, 0} ,
    {PJMEDIA_FORMAT_BGRA,  (Uint32)SDL_PIXELFORMAT_ARGB8888,
     0xFF0000, 0xFF00, 0xFF, 0xFF000000} ,
#endif /* PJ_IS_BIG_ENDIAN */

    {PJMEDIA_FORMAT_DIB , (Uint32)SDL_PIXELFORMAT_RGB24,
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
    struct sdl_factory	    *sf;
    pjmedia_event_type       ev_type;
    pj_status_t              status;
}

- (void)sdl_init;
- (void)sdl_quit;
- (void)detect_fmt_change;
- (void)sdl_create;
- (void)sdl_destroy;
- (void)handle_event;
- (void)put_frame;
@end
#endif /* PJ_DARWINOS */

/* sdl_ device info */
struct sdl_dev_info
{
    pjmedia_vid_dev_info	 info;
};

/* Linked list of streams */
struct stream_list
{
    PJ_DECL_LIST_MEMBER(struct stream_list);
    struct sdl_stream	*stream;
};

/* sdl_ factory */
struct sdl_factory
{
    pjmedia_vid_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;

    unsigned			 dev_count;
    struct sdl_dev_info	        *dev_info;

    pj_thread_t			*sdl_thread;        /**< SDL thread.        */
    pj_status_t                  status;
    pj_sem_t                    *sem;
    pj_mutex_t			*mutex;
    struct stream_list		 streams;
    pj_bool_t                    is_quitting;
};

/* Video stream. */
struct sdl_stream
{
    pjmedia_vid_dev_stream	 base;		    /**< Base stream	    */
    pjmedia_vid_dev_param	 param;		    /**< Settings	    */
    pj_pool_t			*pool;              /**< Memory pool.       */

    pjmedia_vid_dev_cb		 vid_cb;            /**< Stream callback.   */
    void			*user_data;         /**< Application data.  */

    pj_thread_t			*sdl_thread;        /**< SDL thread.        */
    pj_bool_t                    is_initialized;
    pj_bool_t			 is_quitting;
    pj_bool_t			 is_destroyed;
    pj_bool_t			 is_running;
    pj_bool_t			 render_exited;
    pj_status_t			 status;
    pjmedia_format              *new_fmt;
    pjmedia_rect_size           *new_disp_size;
    pj_timestamp		 last_ts;
    pjmedia_frame                frame;
    pj_size_t			 frame_buf_size;
    struct stream_list		 list_entry;
    struct sdl_factory          *sf;

#if SDL_VERSION_ATLEAST(1,3,0)
    SDL_Window                  *window;            /**< Display window.    */
    SDL_Renderer                *renderer;          /**< Display renderer.  */
    SDL_Texture                 *scr_tex;           /**< Screen texture.    */
    int                          pitch;             /**< Pitch value.       */
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
    SDL_Rect			 rect;              /**< Frame rectangle.   */
    SDL_Rect			 dstrect;           /**< Display rectangle. */
    SDL_Surface			*screen;            /**< Display screen.    */
    SDL_Surface			*surf;              /**< RGB surface.       */
    SDL_Overlay			*overlay;           /**< YUV overlay.       */
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#   if SDL_VERSION_ATLEAST(1,3,0)
    SDL_GLContext               *gl_context;
#   endif /* SDL_VERSION_ATLEAST(1,3,0) */ 
    GLuint			 texture;
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

    pjmedia_video_apply_fmt_param vafp;
};

struct sdl_dev_t
{
    struct sdl_factory *sf;
    struct sdl_stream  *strm;
};

/* Prototypes */
static pj_status_t sdl_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t sdl_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t sdl_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    sdl_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t sdl_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					    unsigned index,
					    pjmedia_vid_dev_info *info);
static pj_status_t sdl_factory_default_param(pj_pool_t *pool,
                                             pjmedia_vid_dev_factory *f,
					     unsigned index,
					     pjmedia_vid_dev_param *param);
static pj_status_t sdl_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_dev_param *param,
					const pjmedia_vid_dev_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t sdl_stream_get_param(pjmedia_vid_dev_stream *strm,
					pjmedia_vid_dev_param *param);
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

static int sdl_thread(void * data);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &sdl_factory_init,
    &sdl_factory_destroy,
    &sdl_factory_get_dev_count,
    &sdl_factory_get_dev_info,
    &sdl_factory_default_param,
    &sdl_factory_create_stream,
    &sdl_factory_refresh
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
    struct sdl_dev_t sdl_dev;
    pj_status_t status;
    SDL_version version;

    pj_list_init(&sf->streams);
    status = pj_mutex_create_recursive(sf->pool, "sdl_factory",
				       &sf->mutex);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sem_create(sf->pool, NULL, 0, 1, &sf->sem);
    if (status != PJ_SUCCESS)
	return status;

    sf->status = PJ_EUNKNOWN;
    sdl_dev.sf = sf;
    sdl_dev.strm = NULL;
    status = pj_thread_create(sf->pool, "sdl_thread", sdl_thread,
			      &sdl_dev, 0, 0, &sf->sdl_thread);
    if (status != PJ_SUCCESS) {
        return PJMEDIA_EVID_INIT;
    }

    while (sf->status == PJ_EUNKNOWN)
        pj_thread_sleep(10);

    if (sf->status != PJ_SUCCESS)
        return sf->status;

    sf->dev_count = 1;
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    sf->dev_count++;
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
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
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

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
#endif /* SDL_VERSION_ATLEAST(1,3,0) */

        for (j = 0; j < ddi->info.fmt_cnt; j++) {
            pjmedia_format *fmt = &ddi->info.fmt[j];
            pjmedia_format_init_video(fmt, sdl_fmts[j].fmt_id,
                                      DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                      DEFAULT_FPS, 1);
        }
    }

    SDL_VERSION(&version);
    PJ_LOG(4, (THIS_FILE, "SDL %d.%d initialized",
			  version.major, version.minor));

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t sdl_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    pj_pool_t *pool = sf->pool;

    pj_assert(pj_list_empty(&sf->streams));

    sf->is_quitting = PJ_TRUE;
    if (sf->sdl_thread) {
        pj_sem_post(sf->sem);
        pj_thread_join(sf->sdl_thread);
    }

    if (sf->mutex) {
	pj_mutex_destroy(sf->mutex);
	sf->mutex = NULL;
    }

    if (sf->sem) {
        pj_sem_destroy(sf->sem);
        sf->sem = NULL;
    }

    sf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t sdl_factory_refresh(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
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
					     pjmedia_vid_dev_param *param)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    struct sdl_dev_info *di = &sf->dev_info[index];

    PJ_ASSERT_RETURN(index < sf->dev_count, PJMEDIA_EVID_INVDEV);
    
    PJ_UNUSED_ARG(pool);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_RENDER;
    param->rend_id = index;
    param->cap_id = PJMEDIA_VID_INVALID_DEV;

    /* Set the device capabilities here */
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->fmt.type = PJMEDIA_TYPE_VIDEO;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

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

static void sdl_destroy(struct sdl_stream *strm, pj_bool_t destroy_win)
{
    PJ_UNUSED_ARG(destroy_win);

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
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
#if SDL_VERSION_ATLEAST(1,3,0)
#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->gl_context) {
        SDL_GL_DeleteContext(strm->gl_context);
        strm->gl_context = NULL;
    }
#   endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
    if (strm->scr_tex) {
        SDL_DestroyTexture(strm->scr_tex);
        strm->scr_tex = NULL;
    }
    if (strm->renderer) {
        SDL_DestroyRenderer(strm->renderer);
        strm->renderer = NULL;
    }
#   if !defined(TARGET_OS_IPHONE) || TARGET_OS_IPHONE == 0
    if (destroy_win) {
        if (strm->window &&
            !(strm->param.flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW))
        {
            SDL_DestroyWindow(strm->window);
        }
        strm->window = NULL;
    }
#   endif /* TARGET_OS_IPHONE */
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
}

static pj_status_t sdl_create_view(struct sdl_stream *strm,
				   pjmedia_format *fmt)
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
    if (strm->param.disp_size.w == 0)
        strm->param.disp_size.w = strm->rect.w;
    if (strm->param.disp_size.h == 0)
        strm->param.disp_size.h = strm->rect.h;
    strm->dstrect.x = strm->dstrect.y = 0;
    strm->dstrect.w = (Uint16)strm->param.disp_size.w;
    strm->dstrect.h = (Uint16)strm->param.disp_size.h;

    sdl_destroy(strm, PJ_FALSE);

#if SDL_VERSION_ATLEAST(1,3,0)
    if (!strm->window) {
        Uint32 flags = SDL_WINDOW_SHOWN | /*SDL_WINDOW_RESIZABLE*/
		       SDL_WINDOW_BORDERLESS;

#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
        if (strm->param.rend_id == OPENGL_DEV_IDX)
            flags |= SDL_WINDOW_OPENGL;
#   endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

        if (strm->param.flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
            /* Use the window supplied by the application. */
	    strm->window = SDL_CreateWindowFrom(strm->param.window.info.window);
        } else {
            /* Create the window where we will draw. */
            strm->window = SDL_CreateWindow("pjmedia-SDL video",
                                            SDL_WINDOWPOS_CENTERED,
                                            SDL_WINDOWPOS_CENTERED,
                                            strm->param.disp_size.w,
                                            strm->param.disp_size.h,
                                            flags);
        }
        if (!strm->window)
            return PJMEDIA_EVID_SYSERR;
    }

    SDL_SetWindowSize(strm->window, strm->param.disp_size.w,
                      strm->param.disp_size.h);

    /**
      * We must call SDL_CreateRenderer in order for draw calls to
      * affect this window.
      */
    strm->renderer = SDL_CreateRenderer(strm->window, -1, 0);
    if (!strm->renderer)
        return PJMEDIA_EVID_SYSERR;

#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
        strm->gl_context = SDL_GL_CreateContext(strm->window);
        if (!strm->gl_context)
            return PJMEDIA_EVID_SYSERR;
        SDL_GL_MakeCurrent(strm->window, strm->gl_context);
    }
#   endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

    strm->screen = SDL_GetWindowSurface(strm->window);

#else /* SDL_VERSION_ATLEAST(1,3,0) */
    /* Initialize the display */
    strm->screen = SDL_SetVideoMode(strm->param.disp_size.w, 
                                    strm->param.disp_size.h, 0, (
#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
                                    strm->param.rend_id == OPENGL_DEV_IDX?
				    SDL_OPENGL | SDL_RESIZABLE:
#   endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
				    SDL_RESIZABLE | SDL_SWSURFACE));
    if (strm->screen == NULL)
        return PJMEDIA_EVID_SYSERR;

    SDL_WM_SetCaption("pjmedia-SDL video", NULL);

#endif /* SDL_VERSION_ATLEAST(1,3,0) */

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
	/* Init some OpenGL settings */
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	
	/* Init the viewport */
	glViewport(0, 0, strm->param.disp_size.w, strm->param.disp_size.h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	glOrtho(0.0, (GLdouble)strm->param.disp_size.w,
                (GLdouble)strm->param.disp_size.h, 0.0, 0.0, 1.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	/* Create a texture */
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glGenTextures(1, &strm->texture);

        if (!strm->texture)
            return PJMEDIA_EVID_SYSERR;
    } else
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
#if SDL_VERSION_ATLEAST(1,3,0)
    {    
        strm->scr_tex = SDL_CreateTexture(strm->renderer, sdl_info->sdl_format,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          strm->rect.w, strm->rect.h);
        if (strm->scr_tex == NULL)
            return PJMEDIA_EVID_SYSERR;
    
        strm->pitch = strm->rect.w * SDL_BYTESPERPIXEL(sdl_info->sdl_format);
    }
#else /* SDL_VERSION_ATLEAST(1,3,0) */
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
#endif /* SDL_VERSION_ATLEAST(1,3,0) */

    if (strm->vafp.framebytes > strm->frame_buf_size) {
        strm->frame_buf_size = strm->vafp.framebytes;
        strm->frame.buf = pj_pool_alloc(strm->pool, strm->vafp.framebytes);
    }

    return PJ_SUCCESS;
}

static pj_status_t sdl_create(struct sdl_stream *strm)
{
    strm->is_initialized = PJ_TRUE;

#if !(SDL_VERSION_ATLEAST(1,3,0))
    if (SDL_Init(SDL_INIT_VIDEO)) {
        strm->status = PJMEDIA_EVID_INIT;
        return strm->status;
    }
#endif /* !(SDL_VERSION_ATLEAST(1,3,0)) */

#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    if (strm->param.rend_id == OPENGL_DEV_IDX) {
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    }
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

    strm->status = sdl_create_view(strm, &strm->param.fmt);
    return strm->status;
}

static void detect_fmt_change(struct sdl_stream *strm)
{
    strm->status = PJ_SUCCESS;
    if (strm->new_fmt || strm->new_disp_size) {
	if (strm->new_disp_size) {
	    pj_memcpy(&strm->param.disp_size, strm->new_disp_size,
                      sizeof(strm->param.disp_size));
#if SDL_VERSION_ATLEAST(1,3,0)
	    if (strm->scr_tex) {
		strm->dstrect.x = strm->dstrect.y = 0;
		strm->dstrect.w = (Uint16)strm->param.disp_size.w;
		strm->dstrect.h = (Uint16)strm->param.disp_size.h;
		SDL_RenderSetViewport(strm->renderer, &strm->dstrect);
		strm->new_fmt = NULL;
		strm->new_disp_size = NULL;
		return;
	    }
#endif
	}

        /* Re-initialize SDL */
        strm->status = sdl_create_view(strm, (strm->new_fmt? strm->new_fmt :
				       &strm->param.fmt));

        if (strm->status == PJ_SUCCESS) {
            if (strm->new_fmt)
                pjmedia_format_copy(&strm->param.fmt, strm->new_fmt);
        }
        strm->new_fmt = NULL;
        strm->new_disp_size = NULL;
    }
}

static pj_status_t put_frame(struct sdl_stream *stream,
			     const pjmedia_frame *frame)
{
    if (!stream->is_running)
        return PJ_SUCCESS;

    if (stream->surf) {
	if (SDL_MUSTLOCK(stream->surf)) {
	    if (SDL_LockSurface(stream->surf) < 0) {
		PJ_LOG(3, (THIS_FILE, "Unable to lock SDL surface"));
		return PJMEDIA_EVID_NOTREADY;
	    }
	}
	
	pj_memcpy(stream->surf->pixels, frame->buf,
		  stream->vafp.framebytes);
	
	if (SDL_MUSTLOCK(stream->surf)) {
	    SDL_UnlockSurface(stream->surf);
	}
#if SDL_VERSION_ATLEAST(1,3,0)
        SDL_UpdateWindowSurface(stream->window);
#else /* SDL_VERSION_ATLEAST(1,3,0) */
	SDL_BlitSurface(stream->surf, NULL, stream->screen, &stream->dstrect);
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
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
	SDL_DisplayYUVOverlay(stream->overlay, &stream->dstrect);
    }
#if SDL_VERSION_ATLEAST(1,3,0)
    else if (stream->scr_tex) {
        SDL_UpdateTexture(stream->scr_tex, NULL, frame->buf, stream->pitch);
        SDL_RenderClear(stream->renderer);
        SDL_RenderCopy(stream->renderer, stream->scr_tex,
		       &stream->rect, &stream->dstrect);
        SDL_RenderPresent(stream->renderer);
    }
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
    else if (stream->param.rend_id == OPENGL_DEV_IDX && stream->texture) {
	glBindTexture(GL_TEXTURE_2D, stream->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     stream->rect.w, stream->rect.h, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, frame->buf);
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 0); glVertex2i(0, 0);
	glTexCoord2f(1, 0); glVertex2i(stream->param.disp_size.w, 0);
	glTexCoord2f(0, 1); glVertex2i(0, stream->param.disp_size.h);
	glTexCoord2f(1, 1);
        glVertex2i(stream->param.disp_size.w, stream->param.disp_size.h);
	glEnd();
#   if SDL_VERSION_ATLEAST(1,3,0)
        SDL_GL_SwapWindow(stream->window);
#   else /* SDL_VERSION_ATLEAST(1,3,0) */
	SDL_GL_SwapBuffers();
#   endif /* SDL_VERSION_ATLEAST(1,3,0) */
    }
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */

    return PJ_SUCCESS;
}

static struct sdl_stream* find_stream(struct sdl_factory *sf,
                                      Uint32 windowID,
                                      pjmedia_event *pevent)
{
    struct stream_list *it, *itBegin;
    struct sdl_stream *strm = NULL;

    itBegin = &sf->streams;
    for (it = itBegin->next; it != itBegin; it = it->next) {
#if SDL_VERSION_ATLEAST(1,3,0)
        if (SDL_GetWindowID(it->stream->window) == windowID)
#else /* SDL_VERSION_ATLEAST(1,3,0) */
        PJ_UNUSED_ARG(windowID);
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
        {
            strm = it->stream;
            break;
        }
    }
 
    if (strm)
        pjmedia_event_init(pevent, PJMEDIA_EVENT_NONE, &strm->last_ts,
		           &strm->base.epub);

    return strm;
}

static int poll_event(struct sdl_factory *sf, pjmedia_event *pevent,
                      struct sdl_stream **strm)
{
    int retval;
    SDL_Event sevent;

    retval = SDL_PollEvent(&sevent);
    if (retval) {
#if !(SDL_VERSION_ATLEAST(1,3,0))
        *strm = find_stream(sf, 0, pevent);
        pj_assert(strm);
#endif /* !(SDL_VERSION_ATLEAST(1,3,0)) */

	switch(sevent.type) {
	    case SDL_MOUSEBUTTONDOWN:
#if SDL_VERSION_ATLEAST(1,3,0)
                *strm = find_stream(sf, sevent.button.windowID, pevent);
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
		pevent->type = PJMEDIA_EVENT_MOUSE_BTN_DOWN;
		break;
#if SDL_VERSION_ATLEAST(1,3,0)
	    case SDL_WINDOWEVENT:
                *strm = find_stream(sf, sevent.window.windowID, pevent);
		switch (sevent.window.event) {
		    case SDL_WINDOWEVENT_RESIZED:
			pevent->type = PJMEDIA_EVENT_WND_RESIZED;
			pevent->data.wnd_resized.new_size.w =
			    sevent.window.data1;
			pevent->data.wnd_resized.new_size.h =
			    sevent.window.data2;
			break;
                    case SDL_WINDOWEVENT_CLOSE:
                        pevent->type = PJMEDIA_EVENT_WND_CLOSING;
                        break;
		}
		break;
#else /* SDL_VERSION_ATLEAST(1,3,0) */
	    case SDL_VIDEORESIZE:
		pevent->type = PJMEDIA_EVENT_WND_RESIZED;
		pevent->data.wnd_resized.new_size.w = sevent.resize.w;
		pevent->data.wnd_resized.new_size.h = sevent.resize.h;
		break;
	    case SDL_QUIT:
		pevent->type = PJMEDIA_EVENT_WND_CLOSING;
		break;
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
	}
    }

    return retval;
}

static struct sdl_stream* handle_event(struct sdl_factory *sf,
                                       struct sdl_stream *rcv_strm,
                                       pjmedia_event_type *ev_type)
{
    struct sdl_stream *strm = NULL;
    pjmedia_event pevent;

    *ev_type = PJMEDIA_EVENT_NONE;
    while (poll_event(sf, &pevent, &strm)) {
        *ev_type = pevent.type;
	if (pevent.type != PJMEDIA_EVENT_NONE && strm &&
            (!rcv_strm || rcv_strm == strm))
        {
	    pjmedia_event_publish(&strm->base.epub, &pevent);

	    switch (pevent.type) {
	    case PJMEDIA_EVENT_WND_RESIZED:
		strm->new_disp_size = &pevent.data.wnd_resized.new_size;
                strm->status = PJ_SUCCESS;
		detect_fmt_change(strm);
                if (strm->status != PJ_SUCCESS)
                    return strm;
		break;

	    case PJMEDIA_EVENT_WND_CLOSING:
		if (pevent.data.wnd_closing.cancel) {
		    /* Cancel the closing operation */
		    break;
		}

		/* Proceed to cleanup SDL. App must still call
		 * pjmedia_dev_stream_destroy() when getting WND_CLOSED
		 * event
		 */
		strm->is_quitting = PJ_TRUE;
		sdl_stream_stop(&strm->base);

                return strm;
	    default:
		/* Just to prevent gcc warning about unused enums */
		break;
	    }
	}
    }

    return strm;
}

static int sdl_thread(void * data)
{
    struct sdl_dev_t *sdl_dev = (struct sdl_dev_t *)data;
    struct sdl_factory *sf = sdl_dev->sf;
    struct sdl_stream *strm = sdl_dev->strm;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    NSAutoreleasePool *apool = [[NSAutoreleasePool alloc] init];
    SDLDelegate *delegate = [[SDLDelegate alloc] init];
#endif /* PJ_DARWINOS */

#if SDL_VERSION_ATLEAST(1,3,0)
#   if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    [delegate performSelectorOnMainThread:@selector(sdl_init) 
	      withObject:nil waitUntilDone:YES];
    if (delegate->status != PJ_SUCCESS)
        goto on_error;
#   else /* PJ_DARWINOS */
    /* Initialize the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO)) {
        sf->status = PJMEDIA_EVID_INIT;
        goto on_error;
    }
#   endif /* PJ_DARWINOS */
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
    sf->status = PJ_SUCCESS;

    while (!sf->is_quitting) {
        struct stream_list *it, *itBegin;
        pjmedia_event_type ev_type;
        struct sdl_stream *ev_stream;

        pj_mutex_lock(sf->mutex);

        if (!strm && pj_list_empty(&sf->streams)) {
            /* Wait until there is any stream. */
            pj_mutex_unlock(sf->mutex);
            pj_sem_wait(sf->sem);
            pj_mutex_lock(sf->mutex);
        }

        itBegin = &sf->streams;
        for (it = itBegin->next; it != itBegin; it = it->next) {
            if ((strm && it->stream != strm) || it->stream->is_quitting)
                continue;

            if (!it->stream->is_initialized) {
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
                delegate->strm = it->stream;
                [delegate performSelectorOnMainThread:@selector(sdl_create)
                          withObject:nil waitUntilDone:YES];
#else /* PJ_DARWINOS */
                sdl_create(it->stream);
#endif /* PJ_DARWINOS */
            }

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
            delegate->strm = it->stream;
            [delegate performSelectorOnMainThread:@selector(detect_fmt_change)
                      withObject:nil waitUntilDone:YES];
            [delegate performSelectorOnMainThread:@selector(put_frame)
                      withObject:nil waitUntilDone:YES];
#else /* PJ_DARWINOS */
            detect_fmt_change(it->stream);
            put_frame(it->stream, &it->stream->frame);
#endif /* PJ_DARWINOS */
        }

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
        delegate->sf = sf;
        delegate->strm = strm;
        [delegate performSelectorOnMainThread:@selector(handle_event)
                  withObject:nil waitUntilDone:YES];
        ev_stream = delegate->strm;
        ev_type = delegate->ev_type;
#else /* PJ_DARWINOS */
        ev_stream = handle_event(sf, strm, &ev_type);
#endif /* PJ_DARWINOS */

        itBegin = &sf->streams;
        for (it = itBegin->next; it != itBegin; it = it->next) {
            if ((strm && it->stream != strm) || !it->stream->is_quitting ||
                it->stream->is_destroyed)
                continue;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
            delegate->strm = it->stream;
            [delegate performSelectorOnMainThread:@selector(sdl_destroy)
                      withObject:nil waitUntilDone:YES];
#   if !(SDL_VERSION_ATLEAST(1,3,0))
            [delegate performSelectorOnMainThread:@selector(sdl_quit)
                      withObject:nil waitUntilDone:YES];
#   endif /* !(SDL_VERSION_ATLEAST(1,3,0)) */
#else /* PJ_DARWINOS */
            sdl_destroy(it->stream, PJ_TRUE);
#   if !(SDL_VERSION_ATLEAST(1,3,0))
            SDL_Quit();
#   endif /* !(SDL_VERSION_ATLEAST(1,3,0)) */
#endif /* PJ_DARWINOS */
            it->stream->screen = NULL;
            it->stream->is_destroyed = PJ_TRUE;

            if (ev_type == PJMEDIA_EVENT_WND_CLOSING &&
                it->stream == ev_stream)
            {
                pjmedia_event p_event;

                pjmedia_event_init(&p_event, PJMEDIA_EVENT_WND_CLOSED,
                                   &it->stream->last_ts,
                                   &it->stream->base.epub);
                pjmedia_event_publish(&it->stream->base.epub, &p_event);

                /*
                 * Note: don't access the stream after this point, it
                 * might have been destroyed
                 */
            }

            if (strm) {
                pj_mutex_unlock(sf->mutex);
                return 0;
            }
        }

        pj_mutex_unlock(sf->mutex);
    }

on_error:
#if SDL_VERSION_ATLEAST(1,3,0)
#   if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    [delegate performSelectorOnMainThread:@selector(sdl_quit) 
              withObject:nil waitUntilDone:YES];
    [delegate release];
    [apool release];
#   else /* PJ_DARWINOS */
    SDL_Quit();
#   endif /* PJ_DARWINOS */
#endif /* SDL_VERSION_ATLEAST(1,3,0) */

    return 0;
}

/* API: Put frame from stream */
static pj_status_t sdl_stream_put_frame(pjmedia_vid_dev_stream *strm,
					const pjmedia_frame *frame)
{
    struct sdl_stream *stream = (struct sdl_stream*)strm;

    stream->last_ts.u64 = frame->timestamp.u64;

    if (!stream->is_running) {
	stream->render_exited = PJ_TRUE;
	return PJ_SUCCESS;
    }

    if (frame->size==0 || frame->buf==NULL ||
	frame->size < stream->vafp.framebytes)
	return PJ_SUCCESS;

    pj_memcpy(stream->frame.buf, frame->buf, stream->vafp.framebytes);

    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t sdl_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_dev_param *param,
					const pjmedia_vid_dev_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm)
{
    struct sdl_factory *sf = (struct sdl_factory*)f;
    pj_pool_t *pool;
    struct sdl_stream *strm;
    pj_status_t status;

    PJ_ASSERT_RETURN(param->dir == PJMEDIA_DIR_RENDER, PJ_EINVAL);

#if !SDL_VERSION_ATLEAST(1,3,0)
    /* Prior to 1.3, SDL does not support multiple renderers. */
    pj_mutex_lock(sf->mutex);
    if (!pj_list_empty(&sf->streams)) {
        pj_mutex_unlock(sf->mutex);
        return PJMEDIA_EVID_NOTREADY;
    }
    pj_mutex_unlock(sf->mutex);
#endif

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(sf->pf, "sdl-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct sdl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->sf = sf;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    pjmedia_event_publisher_init(&strm->base.epub, PJMEDIA_SIG_VID_DEV_SDL);
    pj_list_init(&strm->list_entry);
    strm->list_entry.stream = strm;

    /* Create render stream here */
    if (param->dir & PJMEDIA_DIR_RENDER) {
        struct sdl_dev_t sdl_dev;

	strm->status = PJ_SUCCESS;
        sdl_dev.sf = strm->sf;
        sdl_dev.strm = strm;
        pj_mutex_lock(strm->sf->mutex);
#if !SDL_USE_ONE_THREAD_PER_DISPLAY
        if (pj_list_empty(&strm->sf->streams))
            pj_sem_post(strm->sf->sem);
#endif /* !SDL_USE_ONE_THREAD_PER_DISPLAY */
        pj_list_insert_after(&strm->sf->streams, &strm->list_entry);
        pj_mutex_unlock(strm->sf->mutex);

#if SDL_USE_ONE_THREAD_PER_DISPLAY
        status = pj_thread_create(pool, "sdl_thread", sdl_thread,
				  &sdl_dev, 0, 0, &strm->sdl_thread);
	if (status != PJ_SUCCESS) {
	    goto on_error;
	}
#endif /* SDL_USE_ONE_THREAD_PER_DISPLAY */

	while(strm->status == PJ_SUCCESS && !strm->surf && !strm->overlay
#if SDL_VERSION_ATLEAST(1,3,0)
              && !strm->scr_tex
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
#if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
	      && !strm->texture
#endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
	      )
	{
	    pj_thread_sleep(10);
	}
	if ((status = strm->status) != PJ_SUCCESS) {
	    goto on_error;
	}
    }

    /* Apply the remaining settings */
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
	sdl_stream_set_cap(&strm->base,
			   PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION,
			   &param->window_pos);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
	sdl_stream_set_cap(&strm->base,
			   PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
			   &param->window_hide);
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
					pjmedia_vid_dev_param *pi)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
			   &pi->window) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION,
			   &pi->window_pos) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION;
    }
    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE,
			   &pi->disp_size) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE;
    }
    if (sdl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
			   &pi->window_hide) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE;
    }

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

#if SDL_VERSION_ATLEAST(1,3,0)
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW)
    {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (SDL_GetWindowWMInfo(strm->window, &info)) {
	    pjmedia_vid_dev_hwnd *wnd = (pjmedia_vid_dev_hwnd *)pval;
	    if (info.subsystem == SDL_SYSWM_WINDOWS) {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
		wnd->type = PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS;
		wnd->info.win.hwnd = (void *)info.info.win.window;
#endif
	    } else if (info.subsystem == SDL_SYSWM_X11) {
#if defined(SDL_VIDEO_DRIVER_X11)
		wnd->info.x11.window = (void *)info.info.x11.window;
		wnd->info.x11.display = (void *)info.info.x11.display;
#endif
	    } else if (info.subsystem == SDL_SYSWM_COCOA) {
#if defined(SDL_VIDEO_DRIVER_COCOA)
		wnd->info.cocoa.window = (void *)info.info.cocoa.window;
#endif
	    } else if (info.subsystem == SDL_SYSWM_UIKIT) {
#if defined(SDL_VIDEO_DRIVER_UIKIT)
		wnd->info.ios.window = (void *)info.info.uikit.window;
#endif
	    }
	    return PJ_SUCCESS;
	} else
	    return PJMEDIA_EVID_INVCAP;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        SDL_GetWindowPosition(strm->window, &((pjmedia_coord *)pval)->x,
                              &((pjmedia_coord *)pval)->y);
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE) {
        SDL_GetWindowSize(strm->window, (int *)&((pjmedia_rect_size *)pval)->w,
                          (int *)&((pjmedia_rect_size *)pval)->h);
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
	Uint32 flag = SDL_GetWindowFlags(strm->window);
	*((pj_bool_t *)pval) = (flag | SDL_WINDOW_HIDDEN)? PJ_TRUE: PJ_FALSE;
	return PJ_SUCCESS;
    }
#else /* SDL_VERSION_ATLEAST(1,3,0) */
    PJ_UNUSED_ARG(cap);
#endif /* SDL_VERSION_ATLEAST(1,3,0) */

    return PJMEDIA_EVID_INVCAP;
}

/* API: set capability */
static pj_status_t sdl_stream_set_cap(pjmedia_vid_dev_stream *s,
				      pjmedia_vid_dev_cap cap,
				      const void *pval)
{
    struct sdl_stream *strm = (struct sdl_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

#if SDL_VERSION_ATLEAST(1,3,0)
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        SDL_SetWindowPosition(strm->window, ((pjmedia_coord *)pval)->x,
                              ((pjmedia_coord *)pval)->y);
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        if (*(pj_bool_t *)pval)
            SDL_HideWindow(strm->window);
        else
            SDL_ShowWindow(strm->window);
	return PJ_SUCCESS;
    } else
#endif /* SDL_VERSION_ATLEAST(1,3,0) */
    if (cap == PJMEDIA_VID_DEV_CAP_FORMAT) {
        strm->new_fmt = (pjmedia_format *)pval;
	while (strm->new_fmt)
	    pj_thread_sleep(10);
	
	if (strm->status != PJ_SUCCESS) {
	    pj_status_t status = strm->status;
	    
	    /**
	     * Failed to change the output format. Try to revert
	     * to its original format.
	     */
            strm->new_fmt = &strm->param.fmt;
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
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE) {
        strm->new_disp_size = (pjmedia_rect_size *)pval;
	while (strm->new_disp_size)
	    pj_thread_sleep(10);
	
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
        while (!stream->is_destroyed) {
            pj_thread_sleep(10);
        }
    }

    pj_mutex_lock(stream->sf->mutex);
    if (!pj_list_empty(&stream->list_entry))
	pj_list_erase(&stream->list_entry);
    pj_mutex_unlock(stream->sf->mutex);

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
@implementation SDLDelegate
- (void)sdl_init
{
    if (SDL_Init(SDL_INIT_VIDEO)) {
        PJ_LOG(4, (THIS_FILE, "Cannot initialize SDL"));
        status = PJMEDIA_EVID_INIT;
    }
    status = PJ_SUCCESS;
}

- (void)sdl_quit
{
    SDL_Quit();
}

- (void)detect_fmt_change
{
    detect_fmt_change(strm);
}

- (void)sdl_create
{
    sdl_create(strm);
}

- (void)sdl_destroy
{
    sdl_destroy(strm, PJ_TRUE);
}

- (void)handle_event
{
    strm = handle_event(sf, strm, &ev_type);
}

- (void)put_frame
{
    put_frame(strm, &strm->frame);
}

@end
#endif /* PJ_DARWINOS */

#ifdef _MSC_VER
#   pragma comment( lib, "sdl.lib")
#   if PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL
#	pragma comment(lib, "OpenGL32.lib")
#   endif /* PJMEDIA_VIDEO_DEV_SDL_HAS_OPENGL */
#endif /* _MSC_VER */


#endif	/* PJMEDIA_VIDEO_DEV_HAS_SDL */

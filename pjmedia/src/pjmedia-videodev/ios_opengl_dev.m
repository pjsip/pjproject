/* $Id$ */
/*
 * Copyright (C) 2013-2014 Teluu Inc. (http://www.teluu.com)
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

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL) && \
    PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL != 0

#include <pjmedia-videodev/opengl_dev.h>
#import <UIKit/UIKit.h>

#define THIS_FILE		"ios_opengl_dev.c"

typedef struct iosgl_fmt_info
{
    pjmedia_format_id   pjmedia_format;
    UInt32		iosgl_format;
} iosgl_fmt_info;

/* Supported formats */
static iosgl_fmt_info iosgl_fmts[] =
{
    {PJMEDIA_FORMAT_BGRA, kCVPixelFormatType_32BGRA} ,
};

@interface GLView : UIView
{
@public
    struct iosgl_stream *stream;
}

@end

/* Video stream. */
struct iosgl_stream
{
    pjmedia_vid_dev_stream  base;		/**< Base stream       */
    pjmedia_vid_dev_param   param;		/**< Settings	       */
    pj_pool_t		   *pool;		/**< Memory pool       */
    
    pjmedia_vid_dev_cb	    vid_cb;		/**< Stream callback   */
    void		   *user_data;          /**< Application data  */
    
    pj_status_t             status;
    pj_timestamp            frame_ts;
    unsigned                ts_inc;
    
    gl_buffers                  *gl_buf;
    GLView                      *gl_view;
    EAGLContext                 *ogl_context;
    CVOpenGLESTextureCacheRef    vid_texture;
    CVImageBufferRef             pb;
    CVOpenGLESTextureRef         texture;
};


/* Prototypes */
static pj_status_t iosgl_stream_get_param(pjmedia_vid_dev_stream *strm,
                                          pjmedia_vid_dev_param *param);
static pj_status_t iosgl_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        void *value);
static pj_status_t iosgl_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        const void *value);
static pj_status_t iosgl_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t iosgl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame);
static pj_status_t iosgl_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t iosgl_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_stream_op stream_op =
{
    &iosgl_stream_get_param,
    &iosgl_stream_get_cap,
    &iosgl_stream_set_cap,
    &iosgl_stream_start,
    NULL,
    &iosgl_stream_put_frame,
    &iosgl_stream_stop,
    &iosgl_stream_destroy
};

static iosgl_fmt_info* get_iosgl_format_info(pjmedia_format_id id)
{
    unsigned i;
    
    for (i = 0; i < PJ_ARRAY_SIZE(iosgl_fmts); i++) {
        if (iosgl_fmts[i].pjmedia_format == id)
            return &iosgl_fmts[i];
    }
    
    return NULL;
}

@implementation GLView

+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

- (void) init_buffers
{
    /* Initialize OpenGL ES 2 */
    CAEAGLLayer *eagl_layer = (CAEAGLLayer *)[stream->gl_view layer];
    eagl_layer.opaque = YES;
    eagl_layer.drawableProperties =
    [NSDictionary dictionaryWithObjectsAndKeys:
     [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
     kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
     nil];
    
    stream->ogl_context = [[EAGLContext alloc] initWithAPI:
                           kEAGLRenderingAPIOpenGLES2];
    if (!stream->ogl_context ||
        ![EAGLContext setCurrentContext:stream->ogl_context])
    {
        stream->status = PJMEDIA_EVID_SYSERR;
        return;
    }
    
    /* Create GL buffers */
    pjmedia_vid_dev_opengl_create_buffers(stream->pool, &stream->gl_buf);
    
    [stream->ogl_context renderbufferStorage:GL_RENDERBUFFER
                         fromDrawable:(CAEAGLLayer *)[stream->gl_view layer]];
    
    /* Init GL buffers */
    stream->status = pjmedia_vid_dev_opengl_init_buffers(stream->gl_buf);
}

- (void)render
{
    if (![EAGLContext setCurrentContext:stream->ogl_context]) {
        /* Failed to set context */
        return;
    }
    
    pjmedia_vid_dev_opengl_draw(stream->gl_buf,
        (unsigned int)CVOpenGLESTextureGetTarget(stream->texture),
        (unsigned int)CVOpenGLESTextureGetName(stream->texture));
}

@end

/* API: create stream */
pj_status_t
pjmedia_vid_dev_opengl_imp_create_stream(pj_pool_t *pool,
                                         pjmedia_vid_dev_param *param,
                                         const pjmedia_vid_dev_cb *cb,
                                         void *user_data,
                                         pjmedia_vid_dev_stream **p_vid_strm)
{
    struct iosgl_stream *strm;
    const pjmedia_video_format_detail *vfd;
    pj_status_t status = PJ_SUCCESS;
    iosgl_fmt_info *ifi;
    
    if (!(ifi = get_iosgl_format_info(param->fmt.id)))
        return PJMEDIA_EVID_BADFORMAT;
    
    strm = PJ_POOL_ZALLOC_T(pool, struct iosgl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    
    if (param->dir & PJMEDIA_DIR_RENDER) {
        CVReturn err;
        UIWindow *window;
        CGRect rect;
        
	if ((param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) &&
            param->window.info.ios.window)
        {
            /* Get output window handle provided by the application */
	    window = (UIWindow *)param->window.info.ios.window;
            rect = window.bounds;
        } else {
            rect = CGRectMake(0, 0, strm->param.disp_size.w,
                              strm->param.disp_size.h);
        }
        
	strm->gl_view = [[GLView alloc] initWithFrame:rect];
	if (!strm->gl_view)
	    return PJ_ENOMEM;
        strm->gl_view->stream = strm;

        /* Perform OpenGL buffer initializations in the main thread. */
        strm->status = PJ_SUCCESS;
        [strm->gl_view performSelectorOnMainThread:@selector(init_buffers)
                       withObject:nil waitUntilDone:YES];
        if ((status = strm->status) != PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Unable to create and init OpenGL buffers"));
            goto on_error;
        }
        
        /*  Create a new CVOpenGLESTexture cache */
        err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL,
                                           strm->ogl_context, NULL,
                                           &strm->vid_texture);
        if (err) {
            PJ_LOG(3, (THIS_FILE, "Unable to create OpenGL texture cache %d",
                       err));
            status = PJMEDIA_EVID_SYSERR;
            goto on_error;
        }

        PJ_LOG(4, (THIS_FILE, "iOS OpenGL ES renderer successfully created"));
    }
    
    /* Apply the remaining settings */
    /*
     if (param->flags & PJMEDIA_VID_DEV_CAP_INPUT_SCALE) {
     iosgl_stream_set_cap(&strm->base,
     PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
     &param->fmt);
     }
     */
    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;
    
    return PJ_SUCCESS;
    
on_error:
    iosgl_stream_destroy((pjmedia_vid_dev_stream *)strm);
    
    return status;
}

/* API: Get stream info. */
static pj_status_t iosgl_stream_get_param(pjmedia_vid_dev_stream *s,
                                          pjmedia_vid_dev_param *pi)
{
    struct iosgl_stream *strm = (struct iosgl_stream*)s;
    
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    
    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (iosgl_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                           &pi->window) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    }
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t iosgl_stream_get_cap(pjmedia_vid_dev_stream *s,
                                        pjmedia_vid_dev_cap cap,
                                        void *pval)
{
    struct iosgl_stream *strm = (struct iosgl_stream*)s;
    
    PJ_UNUSED_ARG(strm);
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE) {
        return PJMEDIA_EVID_INVCAP;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        pjmedia_vid_dev_hwnd *wnd = (pjmedia_vid_dev_hwnd *)pval;
        wnd->info.ios.window = strm->gl_view;
        return PJ_SUCCESS;
    } else {
	return PJMEDIA_EVID_INVCAP;
    }
}

/* API: set capability */
static pj_status_t iosgl_stream_set_cap(pjmedia_vid_dev_stream *s,
                                        pjmedia_vid_dev_cap cap,
                                        const void *pval)
{
    struct iosgl_stream *strm = (struct iosgl_stream*)s;
    
    PJ_UNUSED_ARG(strm);
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE) {
	return PJ_SUCCESS;
    }
    
    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t iosgl_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    PJ_UNUSED_ARG(stream);
    
    PJ_LOG(4, (THIS_FILE, "Starting ios opengl stream"));
    
    return PJ_SUCCESS;
}

/* API: Put frame from stream */
static pj_status_t iosgl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    CVReturn err;

    err = CVPixelBufferCreateWithBytes(kCFAllocatorDefault,
                                       stream->param.disp_size.w,
                                       stream->param.disp_size.h,
                                       kCVPixelFormatType_32BGRA,
                                       frame->buf,
                                       stream->param.disp_size.w * 4,
                                       NULL, NULL, NULL, &stream->pb);
    if (err) {
        PJ_LOG(3, (THIS_FILE, "Unable to create pixel buffer %d", err));
        return PJMEDIA_EVID_SYSERR;
    }

    /* Create a CVOpenGLESTexture from the CVImageBuffer */
    err=CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                     stream->vid_texture,
                                                     stream->pb, NULL,
                                                     GL_TEXTURE_2D, GL_RGBA,
                                                     stream->param.disp_size.w,
                                                     stream->param.disp_size.h,
                                                     GL_BGRA,
                                                     GL_UNSIGNED_BYTE,
                                                     0, &stream->texture);
    if (!stream->texture || err) {
        PJ_LOG(3, (THIS_FILE, "Unable to create OpenGL texture %d", err));
        CVPixelBufferRelease(stream->pb);
        return PJMEDIA_EVID_SYSERR;
    }
    
    /* Perform OpenGL drawing in the main thread. */
    [stream->gl_view performSelectorOnMainThread:@selector(render)
                           withObject:nil waitUntilDone:YES];
    //    dispatch_async(dispatch_get_main_queue(),
    //                   ^{[stream->gl_view render];});
    
    [stream->ogl_context presentRenderbuffer:GL_RENDERBUFFER];
 
    /* Flush the CVOpenGLESTexture cache and release the texture */
    CVOpenGLESTextureCacheFlush(stream->vid_texture, 0);
    CFRelease(stream->texture);
    CVPixelBufferRelease(stream->pb);
    
    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t iosgl_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    PJ_UNUSED_ARG(stream);
    
    PJ_LOG(4, (THIS_FILE, "Stopping ios opengl stream"));

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t iosgl_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    iosgl_stream_stop(strm);
    
    if (stream->vid_texture) {
        CFRelease(stream->vid_texture);
        stream->vid_texture = NULL;
    }

    if ([EAGLContext currentContext] == stream->ogl_context)
        [EAGLContext setCurrentContext:nil];
    
    if (stream->ogl_context) {
        [stream->ogl_context release];
        stream->ogl_context = NULL;
    }
    
    if (stream->gl_view) {
        UIView *view = stream->gl_view;
        dispatch_async(dispatch_get_main_queue(),
                      ^{
                          [view removeFromSuperview];
                          [view release];
                       });
        stream->gl_view = NULL;
    }
    
    pjmedia_vid_dev_opengl_destroy_buffers(stream->gl_buf);
    
    pj_pool_release(stream->pool);
    
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL */

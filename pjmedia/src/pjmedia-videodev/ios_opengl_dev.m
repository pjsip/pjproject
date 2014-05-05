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
    pjmedia_rect_size       vid_size;
    
    gl_buffers                  *gl_buf;
    GLView                      *gl_view;
    EAGLContext                 *ogl_context;
    CVOpenGLESTextureCacheRef    vid_texture;
    CVImageBufferRef             pb;
    void                        *pb_addr;
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

int pjmedia_vid_dev_opengl_imp_get_cap(void)
{
    return PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW |
           PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE |
           PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION |
           PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE |
           PJMEDIA_VID_DEV_CAP_ORIENTATION;
}

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
    /* Don't make OpenGLES calls while in the background */
    if ([UIApplication sharedApplication].applicationState ==
        UIApplicationStateBackground)
        return;
    
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
    CGRect rect;
    CVReturn err;
    
    strm = PJ_POOL_ZALLOC_T(pool, struct iosgl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    
    /* If OUTPUT_RESIZE flag is not used, set display size to default */
    if (!(param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE)) {
        pj_bzero(&strm->param.disp_size, sizeof(strm->param.disp_size));
    }
    
    /* Set video format */
    status = iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_FORMAT,
                                  &param->fmt);
    if (status != PJ_SUCCESS)
        goto on_error;
    
    rect = CGRectMake(0, 0, strm->param.disp_size.w, strm->param.disp_size.h);
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
    
    /* Apply the remaining settings */
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                             param->window.info.ios.window);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION,
                             &param->window_pos);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
                             &param->window_hide);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_ORIENTATION) {
        iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                             &param->orient);
    }
    
    PJ_LOG(4, (THIS_FILE, "iOS OpenGL ES renderer successfully created"));
                    
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
    
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
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
    
    if (cap==PJMEDIA_VID_DEV_CAP_FORMAT) {
        const pjmedia_video_format_info *vfi;
        pjmedia_video_format_detail *vfd;
        pjmedia_format *fmt = (pjmedia_format *)pval;
        iosgl_fmt_info *ifi;
        
        if (!(ifi = get_iosgl_format_info(fmt->id)))
            return PJMEDIA_EVID_BADFORMAT;
        
        vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                            fmt->id);
        if (!vfi)
            return PJMEDIA_EVID_BADFORMAT;
        
        pjmedia_format_copy(&strm->param.fmt, fmt);
        
        vfd = pjmedia_format_get_video_format_detail(fmt, PJ_TRUE);
        pj_memcpy(&strm->vid_size, &vfd->size, sizeof(vfd->size));
        if (strm->param.disp_size.w == 0 || strm->param.disp_size.h == 0)
            pj_memcpy(&strm->param.disp_size, &vfd->size, sizeof(vfd->size));
        
        /* Invalidate the buffer */
        if (strm->pb) {
            CVPixelBufferRelease(strm->pb);
            strm->pb = NULL;
        }
        
	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        UIView *view = (UIView *)pval;
        strm->param.window.info.ios.window = (void *)pval;
        dispatch_async(dispatch_get_main_queue(),
                       ^{[view addSubview:strm->gl_view];});
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE) {
        pj_memcpy(&strm->param.disp_size, pval, sizeof(strm->param.disp_size));
        dispatch_async(dispatch_get_main_queue(), ^{
            strm->gl_view.bounds = CGRectMake(0, 0, strm->param.disp_size.w,
                                              strm->param.disp_size.h);
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        pj_memcpy(&strm->param.window_pos, pval, sizeof(strm->param.window_pos));
        dispatch_async(dispatch_get_main_queue(), ^{
            strm->gl_view.center = CGPointMake(strm->param.window_pos.x +
                                               strm->param.disp_size.w/2.0,
                                               strm->param.window_pos.y +
                                               strm->param.disp_size.h/2.0);
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        dispatch_async(dispatch_get_main_queue(), ^{
            strm->gl_view.hidden = (BOOL)(*((pj_bool_t *)pval));
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_ORIENTATION) {
        pj_memcpy(&strm->param.orient, pval, sizeof(strm->param.orient));
        if (strm->param.orient == PJMEDIA_ORIENT_UNKNOWN)
            return PJ_SUCCESS;
        dispatch_async(dispatch_get_main_queue(), ^{
            strm->gl_view.transform =
                CGAffineTransformMakeRotation(((int)strm->param.orient-1) *
                                              -M_PI_2);
        });
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

    /* Pixel buffer will only create a wrapper for the frame's buffer,
     * so if the frame buffer changes, we have to recreate pb
     */
    if (!stream->pb || (frame->buf && stream->pb_addr != frame->buf)) {
        if (stream->pb) {
            CVPixelBufferRelease(stream->pb);
            stream->pb = NULL;
        }
        err = CVPixelBufferCreateWithBytes(kCFAllocatorDefault,
                                           stream->vid_size.w,
                                           stream->vid_size.h,
                                           kCVPixelFormatType_32BGRA,
                                           frame->buf,
                                           stream->vid_size.w * 4,
                                           NULL, NULL, NULL, &stream->pb);
        if (err) {
            PJ_LOG(3, (THIS_FILE, "Unable to create pixel buffer %d", err));
            return PJMEDIA_EVID_SYSERR;
        }
        stream->pb_addr = frame->buf;
    }

    /* Create a CVOpenGLESTexture from the CVImageBuffer */
    err=CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                     stream->vid_texture,
                                                     stream->pb, NULL,
                                                     GL_TEXTURE_2D, GL_RGBA,
                                                     stream->vid_size.w,
                                                     stream->vid_size.h,
                                                     GL_BGRA,
                                                     GL_UNSIGNED_BYTE,
                                                     0, &stream->texture);
    if (!stream->texture || err) {
        PJ_LOG(3, (THIS_FILE, "Unable to create OpenGL texture %d", err));
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
    
    if (stream->pb) {
        CVPixelBufferRelease(stream->pb);
        stream->pb = NULL;
    }
    
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

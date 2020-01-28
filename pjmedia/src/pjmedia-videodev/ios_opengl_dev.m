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
#include <pj/os.h>

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL) && \
    PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL != 0

#include <pjmedia-videodev/opengl_dev.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#import <UIKit/UIKit.h>

#define THIS_FILE		"ios_opengl_dev.c"

/* If this is enabled, iOS OpenGL will not return error during creation when
 * in the background. Instead, it will perform the initialization later
 * during rendering.
 */ 
#define ALLOW_DELAYED_INITIALIZATION 	0

typedef struct iosgl_fmt_info
{
    pjmedia_format_id   pjmedia_format;
} iosgl_fmt_info;

/* Supported formats */
static iosgl_fmt_info iosgl_fmts[] =
{
    {PJMEDIA_FORMAT_BGRA} ,
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
    
    pj_bool_t		    is_running;
    pj_status_t             status;
    pj_timestamp            frame_ts;
    unsigned                ts_inc;
    pjmedia_rect_size       vid_size;
    const pjmedia_frame    *frame;
    
    gl_buffers             *gl_buf;
    GLView                 *gl_view;
    EAGLContext            *ogl_context;
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

static void dispatch_sync_on_main_queue(void (^block)(void))
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

@implementation GLView

+ (Class) layerClass
{
    return [CAEAGLLayer class];
}

- (void) init_gl
{
    /* Initialize OpenGL ES 2 */
    CAEAGLLayer *eagl_layer = (CAEAGLLayer *)[stream->gl_view layer];
    eagl_layer.opaque = YES;
    eagl_layer.drawableProperties =
    [NSDictionary dictionaryWithObjectsAndKeys:
     [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
     kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
     nil];
    
    /* EAGLContext initialization will crash if we are in background mode */
    if ([UIApplication sharedApplication].applicationState ==
        UIApplicationStateBackground) {
        stream->status = PJMEDIA_EVID_INIT;
        return;
    }
    
    stream->ogl_context = [[EAGLContext alloc] initWithAPI:
                           kEAGLRenderingAPIOpenGLES2];
    if (!stream->ogl_context ||
        ![EAGLContext setCurrentContext:stream->ogl_context])
    {
        NSLog(@"Failed in initializing EAGLContext");
        stream->status = PJMEDIA_EVID_SYSERR;
        return;
    }
    
    /* Create GL buffers */
    pjmedia_vid_dev_opengl_create_buffers(stream->pool, PJ_FALSE,
                                          &stream->gl_buf);
    
    [stream->ogl_context renderbufferStorage:GL_RENDERBUFFER
                         fromDrawable:(CAEAGLLayer *)[stream->gl_view layer]];
    
    /* Init GL buffers */
    stream->status = pjmedia_vid_dev_opengl_init_buffers(stream->gl_buf);
}

- (void)deinit_gl
{
    if ([EAGLContext currentContext] == stream->ogl_context)
        [EAGLContext setCurrentContext:nil];
    
    if (stream->ogl_context) {
        [stream->ogl_context release];
        stream->ogl_context = NULL;
    }
    
    if (stream->gl_buf) {
        pjmedia_vid_dev_opengl_destroy_buffers(stream->gl_buf);
        stream->gl_buf = NULL;
    }
    
    [self removeFromSuperview];
}

- (void)render
{
    /* Don't make OpenGLES calls while in the background */
    if ([UIApplication sharedApplication].applicationState ==
        UIApplicationStateBackground)
    {
        return;
    }

#if ALLOW_DELAYED_INITIALIZATION
    if (stream->status != PJ_SUCCESS) {
        if (stream->status == PJMEDIA_EVID_INIT) {
            [self init_gl];
            NSLog(@"Initializing OpenGL now %s", stream->status == PJ_SUCCESS?
            	  "success": "failed");
        }
        
        return;
    }
#endif

    if (![EAGLContext setCurrentContext:stream->ogl_context]) {
        /* Failed to set context */
        return;
    }
    
    pjmedia_vid_dev_opengl_draw(stream->gl_buf, stream->vid_size.w, stream->vid_size.h,
                                stream->frame->buf);

    [stream->ogl_context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)finish_render
{
    /* Do nothing. This function is serialized in the main thread, so when
     * it is called, we can be sure that render() has completed.
     */
}

- (void)change_format
{
    pjmedia_video_format_detail *vfd;
    
    vfd = pjmedia_format_get_video_format_detail(&stream->param.fmt, PJ_TRUE);
    pj_memcpy(&stream->vid_size, &vfd->size, sizeof(vfd->size));
    if (stream->param.disp_size.w == 0 || stream->param.disp_size.h == 0)
        pj_memcpy(&stream->param.disp_size, &vfd->size, sizeof(vfd->size));
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
    
    strm = PJ_POOL_ZALLOC_T(pool, struct iosgl_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    
    rect = CGRectMake(0, 0, strm->param.disp_size.w, strm->param.disp_size.h);
    dispatch_sync_on_main_queue(^{
	strm->gl_view = [[GLView alloc] initWithFrame:rect];
    });
    if (!strm->gl_view)
        return PJ_ENOMEM;
    strm->gl_view->stream = strm;

    /* If OUTPUT_RESIZE flag is not used, set display size to default */
    if (!(param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE)) {
        pj_bzero(&strm->param.disp_size, sizeof(strm->param.disp_size));
    }
    
    /* Set video format */
    status = iosgl_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_FORMAT,
                                  &param->fmt);
    if (status != PJ_SUCCESS)
        goto on_error;
    
    /* Perform OpenGL buffer initializations in the main thread. */
    strm->status = PJ_SUCCESS;
    [strm->gl_view performSelectorOnMainThread:@selector(init_gl)
                                    withObject:nil waitUntilDone:YES];
    if ((status = strm->status) != PJ_SUCCESS)
    {
        if (status == PJMEDIA_EVID_INIT) {
            PJ_LOG(3, (THIS_FILE, "Failed to initialize iOS OpenGL because "
            			  "we are in background"));
#if !ALLOW_DELAYED_INITIALIZATION
            goto on_error;
#endif
	} else {
            PJ_LOG(3, (THIS_FILE, "Unable to create and init OpenGL buffers"));
            goto on_error;
        }
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
        pjmedia_format *fmt = (pjmedia_format *)pval;
        iosgl_fmt_info *ifi;
        
        if (!(ifi = get_iosgl_format_info(fmt->id)))
            return PJMEDIA_EVID_BADFORMAT;
        
        vfi = pjmedia_get_video_format_info(pjmedia_video_format_mgr_instance(),
                                            fmt->id);
        if (!vfi)
            return PJMEDIA_EVID_BADFORMAT;
        
        pjmedia_format_copy(&strm->param.fmt, fmt);
        
        [strm->gl_view performSelectorOnMainThread:@selector(change_format)
                       withObject:nil waitUntilDone:YES];

	return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        UIView *view = (UIView *)pval;
        strm->param.window.info.ios.window = (void *)pval;
        dispatch_sync_on_main_queue(^{[view addSubview:strm->gl_view];});
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE) {
        pj_memcpy(&strm->param.disp_size, pval, sizeof(strm->param.disp_size));
        dispatch_sync_on_main_queue(^{
            strm->gl_view.bounds = CGRectMake(0, 0, strm->param.disp_size.w,
                                              strm->param.disp_size.h);
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        pj_memcpy(&strm->param.window_pos, pval, sizeof(strm->param.window_pos));
        dispatch_sync_on_main_queue(^{
            strm->gl_view.center = CGPointMake(strm->param.window_pos.x +
                                               strm->param.disp_size.w/2.0,
                                               strm->param.window_pos.y +
                                               strm->param.disp_size.h/2.0);
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        dispatch_sync_on_main_queue(^{
            strm->gl_view.hidden = (BOOL)(*((pj_bool_t *)pval));
        });
        return PJ_SUCCESS;
    } else if (cap == PJMEDIA_VID_DEV_CAP_ORIENTATION) {
        pj_memcpy(&strm->param.orient, pval, sizeof(strm->param.orient));
        if (strm->param.orient == PJMEDIA_ORIENT_UNKNOWN)
            return PJ_SUCCESS;
        dispatch_sync_on_main_queue(^{
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
    
    PJ_LOG(4, (THIS_FILE, "Starting ios opengl stream"));
    stream->is_running = PJ_TRUE;
    
    return PJ_SUCCESS;
}

/* API: Put frame from stream */
static pj_status_t iosgl_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    /* Video conference just trying to send heart beat for updating timestamp
     * or keep-alive, this port doesn't need any, just ignore.
     */
    if (frame->size==0 || frame->buf==NULL)
	return PJ_SUCCESS;
	
    if (!stream->is_running)
	return PJ_EINVALIDOP;
    
    stream->frame = frame;
    /* Perform OpenGL drawing in the main thread. */
    [stream->gl_view performSelectorOnMainThread:@selector(render)
                           withObject:nil waitUntilDone:YES];
    
    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t iosgl_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    PJ_LOG(4, (THIS_FILE, "Stopping ios opengl stream"));
    stream->is_running = PJ_FALSE;

    /* Wait until the rendering finishes */
    [stream->gl_view performSelectorOnMainThread:@selector(finish_render)
                     withObject:nil waitUntilDone:YES];

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t iosgl_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct iosgl_stream *stream = (struct iosgl_stream*)strm;
    
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    if (stream->is_running)
        iosgl_stream_stop(strm);
    
    if (stream->gl_view) {
        [stream->gl_view performSelectorOnMainThread:@selector(deinit_gl)
              		 withObject:nil waitUntilDone:YES];

        [stream->gl_view release];
        stream->gl_view = NULL;
    }
    
    pj_pool_release(stream->pool);
    
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL */

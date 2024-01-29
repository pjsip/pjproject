/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#include "util.h"
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_METAL) && PJMEDIA_VIDEO_DEV_HAS_METAL != 0

#import "MetalKit/MetalKit.h"
#include "TargetConditionals.h"

#define THIS_FILE               "metal_dev.m"
#define DEFAULT_CLOCK_RATE      90000
#define DEFAULT_WIDTH           640
#define DEFAULT_HEIGHT          480
#define DEFAULT_FPS             15

#if TARGET_OS_IPHONE
#   define NSView   UIView
#   define NSWindow UIWindow
#endif

typedef struct metal_fmt_info
{
    pjmedia_format_id   pjmedia_format;
    MTLPixelFormat      metal_format;
} metal_fmt_info;

static metal_fmt_info metal_fmts[] =
{
    { PJMEDIA_FORMAT_BGRA, MTLPixelFormatBGRA8Unorm },
    { PJMEDIA_FORMAT_RGBA, MTLPixelFormatRGBA8Unorm },
};

struct metal_dev_info
{
    pjmedia_vid_dev_info         info;
};

/* metal factory */
struct metal_factory
{
    pjmedia_vid_dev_factory      base;
    pj_pool_t                   *pool;
    pj_pool_factory             *pf;

    unsigned                     dev_count;
    struct metal_dev_info        dev_info[1];
};

@interface MetalRenderer : NSObject<MTKViewDelegate>
@end

/* Video stream. */
struct metal_stream
{
    pjmedia_vid_dev_stream  base;               /**< Base stream       */
    pjmedia_vid_dev_param   param;              /**< Settings          */
    pj_pool_t              *pool;               /**< Memory pool       */
    struct metal_factory  *factory;             /**< Factory           */

    pjmedia_vid_dev_cb      vid_cb;             /**< Stream callback   */
    void                   *user_data;          /**< Application data  */

    pjmedia_rect_size       size;
    unsigned                bytes_per_row;
    unsigned                frame_size;         /**< Frame size (bytes)*/
    pj_bool_t               is_planar;
    
    pjmedia_vid_dev_conv    conv;
    pjmedia_rect_size       vid_size;
    
    MetalRenderer          *renderer;
    MTKView                *view;
    MTLPixelFormat          format;

    NSWindow               *window;

    pj_bool_t               is_running;
    pj_bool_t               is_rendering;
    void                   *render_buf;
    pj_size_t               render_buf_size;

    pj_timestamp            frame_ts;
    unsigned                ts_inc;
};


/* Prototypes */
static pj_status_t metal_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t metal_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t metal_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    metal_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t metal_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                              unsigned index,
                                              pjmedia_vid_dev_info *info);
static pj_status_t metal_factory_default_param(pj_pool_t *pool,
                                               pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_param *param);
static pj_status_t metal_factory_create_stream(
                                        pjmedia_vid_dev_factory *f,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t metal_stream_get_param(pjmedia_vid_dev_stream *strm,
                                          pjmedia_vid_dev_param *param);
static pj_status_t metal_stream_get_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        void *value);
static pj_status_t metal_stream_set_cap(pjmedia_vid_dev_stream *strm,
                                        pjmedia_vid_dev_cap cap,
                                        const void *value);
static pj_status_t metal_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t metal_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                          const pjmedia_frame *frame);
static pj_status_t metal_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t metal_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &metal_factory_init,
    &metal_factory_destroy,
    &metal_factory_get_dev_count,
    &metal_factory_get_dev_info,
    &metal_factory_default_param,
    &metal_factory_create_stream,
    &metal_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
    &metal_stream_get_param,
    &metal_stream_get_cap,
    &metal_stream_set_cap,
    &metal_stream_start,
    NULL,
    &metal_stream_put_frame,
    &metal_stream_stop,
    &metal_stream_destroy
};

static void dispatch_sync_on_main_queue(void (^block)(void))
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

/****************************************************************************
 * Factory operations
 */
/*
 * Init metal_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_metal_factory(pj_pool_factory *pf)
{
    struct metal_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "metal video", 8000, 4000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct metal_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t metal_factory_init(pjmedia_vid_dev_factory *f)
{
    return metal_factory_refresh(f);
}

/* API: destroy factory */
static pj_status_t metal_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct metal_factory *qf = (struct metal_factory*)f;
    pj_pool_t *pool = qf->pool;

    qf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t metal_factory_refresh(pjmedia_vid_dev_factory *f)
{
    struct metal_factory *qf = (struct metal_factory*)f;

    if (@available(macOS 10.14, iOS 12.0, *)) {
        struct metal_dev_info *qdi;
        unsigned l;
        
        /* Init output device */
        qdi = &qf->dev_info[qf->dev_count++];
        pj_bzero(qdi, sizeof(*qdi));
        pj_ansi_strxcpy(qdi->info.name, "Metal", sizeof(qdi->info.name));
        pj_ansi_strxcpy(qdi->info.driver, "Apple", sizeof(qdi->info.driver));
        qdi->info.dir = PJMEDIA_DIR_RENDER;
        qdi->info.has_callback = PJ_FALSE;
    
        /* Set supported formats */
        qdi->info.caps |= PJMEDIA_VID_DEV_CAP_FORMAT |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE |
                          PJMEDIA_VID_DEV_CAP_ORIENTATION;
        
        for (l = 0; l < PJ_ARRAY_SIZE(metal_fmts); l++) {
            pjmedia_format *fmt = &qdi->info.fmt[qdi->info.fmt_cnt++];
            pjmedia_format_init_video(fmt, metal_fmts[l].pjmedia_format,
                                      DEFAULT_WIDTH, DEFAULT_HEIGHT,
                                      DEFAULT_FPS, 1);
        }
    }
    
    PJ_LOG(4, (THIS_FILE, "Metal video initialized with %d devices",
                          qf->dev_count));

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned metal_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    struct metal_factory *qf = (struct metal_factory*)f;
    return qf->dev_count;
}

/* API: get device info */
static pj_status_t metal_factory_get_dev_info(pjmedia_vid_dev_factory *f,
                                               unsigned index,
                                               pjmedia_vid_dev_info *info)
{
    struct metal_factory *qf = (struct metal_factory*)f;

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &qf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t metal_factory_default_param(pj_pool_t *pool,
                                                pjmedia_vid_dev_factory *f,
                                                unsigned index,
                                                pjmedia_vid_dev_param *param)
{
    struct metal_factory *qf = (struct metal_factory*)f;
    struct metal_dev_info *di;

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);
    PJ_UNUSED_ARG(pool);
    
    di = &qf->dev_info[index];

    pj_bzero(param, sizeof(*param));
    if (di->info.dir & PJMEDIA_DIR_RENDER) {
        param->dir = PJMEDIA_DIR_RENDER;
        param->rend_id = index;
        param->cap_id = PJMEDIA_VID_INVALID_DEV;
    } else {
        return PJMEDIA_EVID_INVDEV;
    }

    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

    return PJ_SUCCESS;
}

@implementation MetalRenderer
{
    MTKView                    *_view;
    id<MTLDevice>               device;
    id<MTLCommandQueue>         commandQueue;    
    id<MTLRenderPipelineState>  pipelineState;
    id<MTLBuffer>               vertexBuffer;
    id<MTLBuffer>               textureCoordBuffer;

@public
    struct metal_stream        *stream;    
}

#define _STRINGIFY( _x ) # _x

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)mtkView
{
    self = [super init];
    if (self) {
        NSString *code;
        NSError *error = nil;
        id<MTLLibrary> shaderLibrary;
        id <MTLFunction> fragmentProgram;
        id <MTLFunction> vertexProgram;
        MTLRenderPipelineDescriptor *pQuadPipelineStateDescriptor;

        /* Create a buffer for a full-screen quad */
        static const float vertexData[] = {
            -1.0, -1.0, 0.0, 1.0,
            1.0, -1.0, 0.0, 1.0,
            -1.0, 1.0, 0.0, 1.0,
            1.0, 1.0, 0.0, 1.0,
        };

        /* Create texture coordinates for a full-screen quad */
        static const float textureCoordinates[] = {
            0.0, 1.0,
            1.0, 1.0,
            0.0, 0.0,
            1.0, 0.0,
        };

        _view = mtkView;
        device = mtkView.device;
        /* Create the command queue */
        commandQueue = [device newCommandQueue];

        /* Metal shader code */
        code = [NSString stringWithFormat:@"#include <metal_stdlib>\n%s",
                _STRINGIFY(
            using namespace metal;

            struct VertexInOut
            {
                float4 m_Position [[position]];
                float2 m_TexCoord [[user(texturecoord)]];
            };

            vertex VertexInOut
            texturedVertex(constant float4        *pPosition  [[ buffer(0) ]],
                           constant packed_float2 *pTexCoords [[ buffer(1) ]],
                           constant float4x4      *pMVP       [[ buffer(2) ]],
                           uint                    vid        [[ vertex_id ]])
            {
                VertexInOut outVertices;

                outVertices.m_Position = pPosition[vid];
                outVertices.m_TexCoord = pTexCoords[vid];

                return outVertices;
            }

            fragment half4
            texturedFrag(VertexInOut     inFrag    [[ stage_in   ]],
                         texture2d<half> tex2D     [[ texture(0) ]])
            {
                constexpr sampler quad_sampler;
                half4 color = tex2D.sample(quad_sampler, inFrag.m_TexCoord);

                return color;
            }
        )];

        /* Create a Metal library instance by compiling the code */
        shaderLibrary = [device newLibraryWithSource:code options:nil
                         error:&error];
        if (error) {
            NSLog(@"Unable to create Metal library err: %@", error);
            return self;
        }

        fragmentProgram = [shaderLibrary newFunctionWithName:@"texturedFrag"];
        vertexProgram = [shaderLibrary newFunctionWithName:@"texturedVertex"];
        if (!fragmentProgram || !vertexProgram) {
            NSLog(@"Unable to load Metal functions");
            return self;
        }

        /* Create a pipeline state */
        pQuadPipelineStateDescriptor = [MTLRenderPipelineDescriptor new];
        pQuadPipelineStateDescriptor.colorAttachments[0].pixelFormat =
            stream->format;
        pQuadPipelineStateDescriptor.vertexFunction  = vertexProgram;
        pQuadPipelineStateDescriptor.fragmentFunction = fragmentProgram;

        pipelineState = [device
            newRenderPipelineStateWithDescriptor:pQuadPipelineStateDescriptor
            error:&error];
        if (error) {
            NSLog(@"newRenderPipelineStateWithDescriptor err: %@", error);
            return self;
        }

        vertexBuffer = [device newBufferWithBytes:vertexData
                               length:sizeof(vertexData)
                               options:MTLResourceStorageModeShared];

        textureCoordBuffer = [device newBufferWithBytes:textureCoordinates
                                     length:sizeof(textureCoordinates)
                                     options:MTLResourceStorageModeShared];
    }

    return self;
}

- (void)dealloc {
    if (vertexBuffer) {
        [vertexBuffer release];
        vertexBuffer = nil;
    }
    if (textureCoordBuffer) {
        [textureCoordBuffer release];
        textureCoordBuffer = nil;
    }
    if (device) {
        [device release];
        device = nil;
    }
    if (commandQueue) {
        [commandQueue release];
        commandQueue = nil;
    }
    if (pipelineState) {
        [pipelineState release];
        pipelineState = nil;
    }

    [super dealloc];
}

- (void)update_image
{
    MTLRenderPassDescriptor *renderPassDescriptor;
    id<MTLCommandBuffer> commandBuffer;
    id<MTLRenderCommandEncoder> renderEncoder;
    id<MTLDrawable> drawable;
    id<MTLTexture> texture;
    MTLTextureDescriptor *textureDescriptor;
    MTLRegion region;
    unsigned width = stream->size.w, height = stream->size.h;

    /* Place the buffer into a texture */
    textureDescriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:stream->format
        width:width height:height mipmapped:NO];

    texture = [device newTextureWithDescriptor:textureDescriptor];
    region = MTLRegionMake2D(0, 0, width, height);
    [texture replaceRegion:region mipmapLevel:0 withBytes:stream->render_buf
             bytesPerRow:stream->bytes_per_row];

    /* The render pass descriptor references the texture into which Metal
     * should draw.
     */
    renderPassDescriptor = _view.currentRenderPassDescriptor;
    if (renderPassDescriptor == nil)
        return;

    commandBuffer = [commandQueue commandBuffer];

    renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:
                                   renderPassDescriptor];
    [renderEncoder setRenderPipelineState:pipelineState];
    [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [renderEncoder setVertexBuffer:textureCoordBuffer offset:0 atIndex:1];

    [renderEncoder setFragmentTexture:texture atIndex:0];
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0
                   vertexCount:4];

    [renderEncoder endEncoding];
    
    /* Get the drawable that will be presented at the end of the frame */
    drawable = _view.currentDrawable;

    /* Request that the drawable texture be presented by the windowing system
     * once drawing is done.
     */
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    stream->is_rendering = PJ_FALSE;
}


/* Called whenever the view needs to render a frame. */
- (void)drawInMTKView:(nonnull MTKView *)view
{
}

/* Called whenever view changes orientation or is resized */
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
}

@end

static pj_status_t metal_init_view(struct metal_stream *strm)
{
    pjmedia_vid_dev_param *param = &strm->param;
    CGRect view_rect = CGRectMake(0, 0, param->fmt.det.vid.size.w,
                                  param->fmt.det.vid.size.h);
    
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE) {
        view_rect.size.width = param->disp_size.w;
        view_rect.size.height = param->disp_size.h;
    }
    
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION) {
        view_rect.origin.x = param->window_pos.x;
        view_rect.origin.y = param->window_pos.y;
    }
    
    strm->view = [[MTKView alloc] initWithFrame:view_rect];
    if (!strm->view)
        return PJMEDIA_EVID_SYSERR;

    strm->view.enableSetNeedsDisplay = NO;
    strm->view.paused = NO;
    strm->view.device = MTLCreateSystemDefaultDevice();
    strm->view.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    strm->param.window.info.window = strm->view;

    strm->renderer = [MetalRenderer alloc];
    if (!strm->renderer)
        return PJ_ENOMEM;

    strm->renderer->stream = strm;
    strm->view.delegate = strm->renderer;
    [strm->renderer initWithMetalKitView:strm->view];

    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        PJ_ASSERT_RETURN(param->window.info.ios.window, PJ_EINVAL);
        metal_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                             param->window.info.ios.window);
    } else {
#if !TARGET_OS_IPHONE
        /* Create the main window */
        strm->window = [[NSWindow alloc] initWithContentRect:view_rect
                                         styleMask:(NSWindowStyleMaskTitled |
                                                    NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskResizable)
                                         backing:NSBackingStoreBuffered
                                         defer:NO];
        if (!strm->window)
            return PJMEDIA_EVID_SYSERR;

        /* Make the window visible */
        [strm->window.contentView addSubview:strm->view];
        [strm->window makeKeyAndOrderFront:strm->window];
        strm->param.window.info.window = strm->window;
#endif
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        metal_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
                           &param->window_hide);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_ORIENTATION) {
        metal_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                           &param->orient);
    }

    return PJ_SUCCESS;
}

static metal_fmt_info* get_metal_format_info(pjmedia_format_id id)
{
    unsigned i;
    
    for (i = 0; i < PJ_ARRAY_SIZE(metal_fmts); i++) {
        if (metal_fmts[i].pjmedia_format == id)
            return &metal_fmts[i];
    }
    
    return NULL;
}

/* API: create stream */
static pj_status_t metal_factory_create_stream(
                                        pjmedia_vid_dev_factory *f,
                                        pjmedia_vid_dev_param *param,
                                        const pjmedia_vid_dev_cb *cb,
                                        void *user_data,
                                        pjmedia_vid_dev_stream **p_vid_strm)
{
    struct metal_factory *qf = (struct metal_factory*)f;
    pj_pool_t *pool;
    struct metal_stream *strm;
    pjmedia_video_format_detail *vfd;
    const pjmedia_video_format_info *vfi;
    metal_fmt_info *mfi;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
                     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
                     param->dir == PJMEDIA_DIR_RENDER,
                     PJ_EINVAL);

    if (!(mfi = get_metal_format_info(param->fmt.id)))
        return PJMEDIA_EVID_BADFORMAT;

    vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
    if (!vfi)
        return PJMEDIA_EVID_BADFORMAT;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(qf->pf, "metal-dev", 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct metal_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    strm->factory = qf;
    
    vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt, PJ_TRUE);
    pj_memcpy(&strm->size, &vfd->size, sizeof(vfd->size));
    strm->bytes_per_row = strm->size.w * vfi->bpp / 8;
    strm->frame_size = strm->bytes_per_row * strm->size.h;
    strm->ts_inc = PJMEDIA_SPF2(param->clock_rate, &vfd->fps, 1);
    strm->is_planar = vfi->plane_cnt > 1;
    strm->format = mfi->metal_format;
        
    if (param->dir & PJMEDIA_DIR_RENDER) {
        /* Create renderer stream here */
        
        dispatch_sync_on_main_queue(^{
            metal_init_view(strm);
        });
                
        strm->render_buf = pj_pool_alloc(pool, strm->frame_size);
        strm->render_buf_size = strm->frame_size;

        PJ_LOG(4, (THIS_FILE, "Metal renderer initialized"));
    }
    
    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;
    
    return PJ_SUCCESS;
    
on_error:
    metal_stream_destroy((pjmedia_vid_dev_stream *)strm);
    
    return status;
}

/* API: Get stream info. */
static pj_status_t metal_stream_get_param(pjmedia_vid_dev_stream *s,
                                           pjmedia_vid_dev_param *pi)
{
    struct metal_stream *strm = (struct metal_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t metal_stream_get_cap(pjmedia_vid_dev_stream *s,
                                         pjmedia_vid_dev_cap cap,
                                         void *pval)
{
    struct metal_stream *strm = (struct metal_stream*)s;
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    PJ_UNUSED_ARG(strm);

    switch (cap) {
        case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
        {
            pjmedia_vid_dev_hwnd *hwnd = (pjmedia_vid_dev_hwnd *) pval;
            hwnd->type = TARGET_OS_IPHONE? PJMEDIA_VID_DEV_HWND_TYPE_IOS:
                         PJMEDIA_VID_DEV_HWND_TYPE_COCOA;
            hwnd->info.window = (void *)strm->view;
            return PJ_SUCCESS;
        }
        default:
            break;
    }
    
    return PJMEDIA_EVID_INVCAP;
}

/* API: set capability */
static pj_status_t metal_stream_set_cap(pjmedia_vid_dev_stream *s,
                                         pjmedia_vid_dev_cap cap,
                                         const void *pval)
{
    struct metal_stream *strm = (struct metal_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
        case PJMEDIA_VID_DEV_CAP_FORMAT:
        {
            const pjmedia_video_format_info *vfi;
            pjmedia_video_format_detail *vfd;
            pjmedia_format *fmt = (pjmedia_format *)pval;
            metal_fmt_info *ifi;
        
            if (!(ifi = get_metal_format_info(fmt->id)))
                return PJMEDIA_EVID_BADFORMAT;
        
            vfi = pjmedia_get_video_format_info(
                                        pjmedia_video_format_mgr_instance(),
                                        fmt->id);
            if (!vfi)
                return PJMEDIA_EVID_BADFORMAT;
        
            pjmedia_format_copy(&strm->param.fmt, fmt);
        
            vfd = pjmedia_format_get_video_format_detail(fmt, PJ_TRUE);
            pj_memcpy(&strm->size, &vfd->size, sizeof(vfd->size));
            strm->bytes_per_row = strm->size.w * vfi->bpp / 8;
            strm->frame_size = strm->bytes_per_row * strm->size.h;
            if (strm->render_buf_size < strm->frame_size) {
                /* Realloc only when needed */
                strm->render_buf = pj_pool_alloc(strm->pool, strm->frame_size);
                strm->render_buf_size = strm->frame_size;
            }
            
            return PJ_SUCCESS;
        }
        
        case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
        {
            strm->param.window.info.window = (void *)pval;
            dispatch_sync_on_main_queue(^{
                [(NSView *)pval addSubview:strm->view];
            });
            return PJ_SUCCESS;
        }
            
        case PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE:
        {
            pj_memcpy(&strm->param.disp_size, pval,
                      sizeof(strm->param.disp_size));
            dispatch_sync_on_main_queue(^{
                CGRect r = (strm->window)? strm->window.frame:
                           strm->view.bounds;
                r.size = CGSizeMake(strm->param.disp_size.w,
                                    strm->param.disp_size.h);
                if (!strm->window)
                    strm->view.bounds = r;
#if !TARGET_OS_IPHONE
                else
                    [strm->window setFrame:r display:YES];
#endif
            });
            return PJ_SUCCESS;
        }
    
        case PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION:
        {
            pj_memcpy(&strm->param.window_pos, pval,
                      sizeof(strm->param.window_pos));
            dispatch_sync_on_main_queue(^{
#if TARGET_OS_IPHONE
                strm->view.center =
                            CGPointMake(strm->param.window_pos.x +
                                        strm->param.disp_size.w/2.0,
                                        strm->param.window_pos.y +
                                        strm->param.disp_size.h/2.0);
#else
                if (strm->window) {
                    [strm->window setFrameOrigin:
                                  NSMakePoint(strm->param.window_pos.x,
                                              strm->param.window_pos.y)];
                } else {
                    [strm->view setFrameOrigin:
                                NSMakePoint(strm->param.window_pos.x,
                                            strm->param.window_pos.y)];
                }
#endif
            });
            return PJ_SUCCESS;
        }
            
        case PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE:
        {
            dispatch_sync_on_main_queue(^{
                pj_bool_t hide = *((pj_bool_t *)pval);
                if (strm->window) {
#if !TARGET_OS_IPHONE
                    if (hide)
                        [strm->window orderOut: nil];
                    else
                        [strm->window orderFront: nil];
#endif
                } else {
                    strm->view.hidden = (BOOL)hide;
                }
            });
            return PJ_SUCCESS;
        }
        
        case PJMEDIA_VID_DEV_CAP_ORIENTATION:
        {
            pjmedia_orient orient = *(pjmedia_orient *)pval;

            pj_assert(orient >= PJMEDIA_ORIENT_UNKNOWN &&
                      orient <= PJMEDIA_ORIENT_ROTATE_270DEG);

            if (orient == PJMEDIA_ORIENT_UNKNOWN)
                return PJ_EINVAL;

            pj_memcpy(&strm->param.orient, pval,
                      sizeof(strm->param.orient));
        
            dispatch_sync_on_main_queue(^{
                CGFloat angle =  -M_PI_2 * ((int)strm->param.orient-1);
#if TARGET_OS_IPHONE
                strm->view.transform =
                    CGAffineTransformMakeRotation(angle);
#else
                strm->view.layer.affineTransform =
                    CGAffineTransformMakeRotation(angle);
#endif
            });

            return PJ_SUCCESS;
        }
        
        default:
            break;
    }

    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t metal_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct metal_stream *stream = (struct metal_stream*)strm;

    PJ_LOG(4, (THIS_FILE, "Starting Metal video stream"));
    stream->is_running = PJ_TRUE;

    return PJ_SUCCESS;
}


/* API: Put frame from stream */
static pj_status_t metal_stream_put_frame(pjmedia_vid_dev_stream *strm,
                                           const pjmedia_frame *frame)
{
    struct metal_stream *stream = (struct metal_stream*)strm;

    /* Video conference just trying to send heart beat for updating timestamp
     * or keep-alive, this port doesn't need any, just ignore.
     */
    if (frame->size==0 || frame->buf==NULL)
        return PJ_SUCCESS;

    if (!stream->is_running)
        return PJ_EINVALIDOP;
    
    /* Prevent more than one async rendering task. */
    if (stream->is_rendering)
        return PJ_EIGNORED;

    if (stream->frame_size >= frame->size)
        pj_memcpy(stream->render_buf, frame->buf, frame->size);
    else
        pj_memcpy(stream->render_buf, frame->buf, stream->frame_size);
    
    /* Perform video display in the main thread */
    stream->is_rendering = PJ_TRUE;
    [stream->renderer performSelectorOnMainThread:@selector(update_image)
                      withObject:nil waitUntilDone:NO];

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t metal_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct metal_stream *stream = (struct metal_stream*)strm;
    unsigned i;
    
    PJ_LOG(4, (THIS_FILE, "Stopping Metal video stream"));

    stream->is_running = PJ_FALSE;

    /* Wait until the rendering finishes */
    for (i = 0; i < 15; i++) pj_thread_sleep(10);
    while (stream->is_rendering);
    
    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t metal_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct metal_stream *stream = (struct metal_stream*)strm;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    metal_stream_stop(strm);

    if (stream->renderer) {
        [stream->renderer release];
        stream->renderer = nil;
    }

    if (stream->view) {
        [stream->view
            performSelectorOnMainThread:@selector(removeFromSuperview)
            withObject:nil waitUntilDone:YES];
        [stream->view release];
        stream->view = nil;
    }

    if (stream->window) {
        [stream->window performSelectorOnMainThread:@selector(close)
                        withObject:nil waitUntilDone:YES];
        stream->window = nil;
    }
    
    pjmedia_vid_dev_conv_destroy_converter(&stream->conv);

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_VIDEO_DEV_HAS_METAL */

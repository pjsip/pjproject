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
#include "util.h"
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0 && \
    defined(PJMEDIA_VIDEO_DEV_HAS_DARWIN) && PJMEDIA_VIDEO_DEV_HAS_DARWIN != 0

#include "TargetConditionals.h"

#if TARGET_OS_IPHONE
    /* On iOS, we supports rendering using UIView */
    #import <UIKit/UIKit.h>
#endif

#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

#define THIS_FILE		"darwin_dev.m"
#define DEFAULT_CLOCK_RATE	90000
#if TARGET_OS_IPHONE
    #define DEFAULT_WIDTH	352
    #define DEFAULT_HEIGHT	288
#else
    #define DEFAULT_WIDTH	1280
    #define DEFAULT_HEIGHT	720
#endif
#define DEFAULT_FPS		15

/* Define whether we should maintain the aspect ratio when rotating the image.
 * For more details, please refer to util.h.
 */
#define MAINTAIN_ASPECT_RATIO 	PJ_TRUE

typedef struct darwin_fmt_info
{
    pjmedia_format_id   pjmedia_format;
    UInt32		darwin_format;
} darwin_fmt_info;

static darwin_fmt_info darwin_fmts[] =
{
#if !TARGET_OS_IPHONE
    { PJMEDIA_FORMAT_YUY2, kCVPixelFormatType_422YpCbCr8_yuvs },
    { PJMEDIA_FORMAT_UYVY, kCVPixelFormatType_422YpCbCr8 },
#endif
    { PJMEDIA_FORMAT_BGRA, kCVPixelFormatType_32BGRA },
    { PJMEDIA_FORMAT_I420, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange }
};

typedef struct darwin_supported_size
{
    pj_size_t supported_size_w;
    pj_size_t supported_size_h;
    NSString *preset_str;
} darwin_supported_size;

/* Set the preset_str on set_preset_str method. */
static darwin_supported_size darwin_sizes[] =
{
#if !TARGET_OS_IPHONE
    { 320, 240, NULL },
#endif
    { 352, 288, NULL },
    { 640, 480, NULL },
#if !TARGET_OS_IPHONE
    { 960, 540, NULL },
#endif
    { 1280, 720, NULL }
#if TARGET_OS_IPHONE
    ,{ 1920, 1080, NULL }
#endif
};

/* darwin device info */
struct darwin_dev_info
{
    pjmedia_vid_dev_info	 info;
    AVCaptureDevice             *dev;
};

/* darwin factory */
struct darwin_factory
{
    pjmedia_vid_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;

    unsigned			 dev_count;
    struct darwin_dev_info	*dev_info;
};

@interface VOutDelegate: NSObject 
			 <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
    struct darwin_stream *stream;
}
@end

/* Video stream. */
struct darwin_stream
{
    pjmedia_vid_dev_stream  base;		/**< Base stream       */
    pjmedia_vid_dev_param   param;		/**< Settings	       */
    pj_pool_t		   *pool;		/**< Memory pool       */
    struct darwin_factory  *factory;            /**< Factory           */

    pjmedia_vid_dev_cb	    vid_cb;		/**< Stream callback   */
    void		   *user_data;          /**< Application data  */

    pjmedia_rect_size	    size;
    unsigned		    bytes_per_row;
    unsigned		    frame_size;         /**< Frame size (bytes)*/
    pj_bool_t               is_planar;
    NSLock 		   *frame_lock;
    void                   *capture_buf;
    void		   *frame_buf;
    
    pjmedia_vid_dev_conv    conv;
    pjmedia_rect_size	    vid_size;
    
    AVCaptureSession		*cap_session;
    AVCaptureDeviceInput	*dev_input;
    pj_bool_t		 	 has_image;
    AVCaptureVideoDataOutput	*video_output;
    VOutDelegate		*vout_delegate;
    dispatch_queue_t 		 queue;
    AVCaptureVideoPreviewLayer  *prev_layer;
    
#if TARGET_OS_IPHONE
    void		*render_buf;
    pj_size_t		 render_buf_size;
    CGDataProviderRef    render_data_provider;
    UIView              *render_view;
#endif

    pj_timestamp	 frame_ts;
    unsigned		 ts_inc;
};


/* Prototypes */
static pj_status_t darwin_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t darwin_factory_destroy(pjmedia_vid_dev_factory *f);
static pj_status_t darwin_factory_refresh(pjmedia_vid_dev_factory *f);
static unsigned    darwin_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t darwin_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					       unsigned index,
					       pjmedia_vid_dev_info *info);
static pj_status_t darwin_factory_default_param(pj_pool_t *pool,
						pjmedia_vid_dev_factory *f,
					     	unsigned index,
					     	pjmedia_vid_dev_param *param);
static pj_status_t darwin_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_dev_param *param,
					const pjmedia_vid_dev_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t darwin_stream_get_param(pjmedia_vid_dev_stream *strm,
				           pjmedia_vid_dev_param *param);
static pj_status_t darwin_stream_get_cap(pjmedia_vid_dev_stream *strm,
				         pjmedia_vid_dev_cap cap,
				         void *value);
static pj_status_t darwin_stream_set_cap(pjmedia_vid_dev_stream *strm,
				         pjmedia_vid_dev_cap cap,
				         const void *value);
static pj_status_t darwin_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t darwin_stream_get_frame(pjmedia_vid_dev_stream *strm,
                                           pjmedia_frame *frame);
static pj_status_t darwin_stream_put_frame(pjmedia_vid_dev_stream *strm,
					   const pjmedia_frame *frame);
static pj_status_t darwin_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t darwin_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &darwin_factory_init,
    &darwin_factory_destroy,
    &darwin_factory_get_dev_count,
    &darwin_factory_get_dev_info,
    &darwin_factory_default_param,
    &darwin_factory_create_stream,
    &darwin_factory_refresh
};

static pjmedia_vid_dev_stream_op stream_op =
{
    &darwin_stream_get_param,
    &darwin_stream_get_cap,
    &darwin_stream_set_cap,
    &darwin_stream_start,
    &darwin_stream_get_frame,
    &darwin_stream_put_frame,
    &darwin_stream_stop,
    &darwin_stream_destroy
};

static void set_preset_str()
{
    int idx = 0;
#if !TARGET_OS_IPHONE
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset320x240;
#endif
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset352x288;
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset640x480;
#if !TARGET_OS_IPHONE
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset960x540;
#endif
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset1280x720;
#if TARGET_OS_IPHONE
    darwin_sizes[idx++].preset_str = AVCaptureSessionPreset1920x1080;
#endif
}

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
 * Init darwin_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_darwin_factory(pj_pool_factory *pf)
{
    struct darwin_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "darwin video", 512, 512, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct darwin_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t darwin_factory_init(pjmedia_vid_dev_factory *f)
{
    return darwin_factory_refresh(f);
}

/* API: destroy factory */
static pj_status_t darwin_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;
    pj_pool_t *pool = qf->pool;

    qf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t darwin_factory_refresh(pjmedia_vid_dev_factory *f)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;
    struct darwin_dev_info *qdi;
    unsigned i, l, first_idx, front_idx = -1;
    enum { MAX_DEV_COUNT = 8 };
    
    set_preset_str();
    
    /* Initialize input and output devices here */
    qf->dev_info = (struct darwin_dev_info*)
		   pj_pool_calloc(qf->pool, MAX_DEV_COUNT,
				  sizeof(struct darwin_dev_info));
    qf->dev_count = 0;
    
#if TARGET_OS_IPHONE
    /* Init output device */
    qdi = &qf->dev_info[qf->dev_count++];
    pj_bzero(qdi, sizeof(*qdi));
    pj_ansi_strncpy(qdi->info.name, "UIView", sizeof(qdi->info.name));
    pj_ansi_strncpy(qdi->info.driver, "iOS", sizeof(qdi->info.driver));
    qdi->info.dir = PJMEDIA_DIR_RENDER;
    qdi->info.has_callback = PJ_FALSE;
#endif
    
    /* Init input device */
    first_idx = qf->dev_count;
    if (NSClassFromString(@"AVCaptureSession")) {
	NSArray *dev_list = NULL;

#if (TARGET_OS_IPHONE && defined(__IPHONE_10_0)) || \
    (TARGET_OS_OSX && defined(__MAC_10_15))
	if (__builtin_available(macOS 10.15, iOS 10.0, *)) {
	    /* Starting in iOS 10 and macOS 10.15, [AVCaptureDevice devices]
	     * is deprecated and replaced by AVCaptureDeviceDiscoverySession.
	     */
    	    AVCaptureDeviceDiscoverySession *dds;
	    NSArray<AVCaptureDeviceType> *dev_types =
	    	@[AVCaptureDeviceTypeBuiltInWideAngleCamera
#if TARGET_OS_OSX && defined(__MAC_10_15)
	    	  , AVCaptureDeviceTypeExternalUnknown
#endif
#if TARGET_OS_IPHONE && defined(__IPHONE_10_0)
	    	  , AVCaptureDeviceTypeBuiltInDuoCamera
	    	  , AVCaptureDeviceTypeBuiltInTelephotoCamera
#endif
	    	  ];

    	    dds = [AVCaptureDeviceDiscoverySession
    	       	   discoverySessionWithDeviceTypes:dev_types
    	           mediaType:AVMediaTypeVideo
    	           position:AVCaptureDevicePositionUnspecified];

    	    dev_list = [dds devices];
	} else {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_15
	    dev_list = [AVCaptureDevice devices];
#endif
	}
#else
	dev_list = [AVCaptureDevice devices];
#endif

        for (AVCaptureDevice *device in dev_list) {
            if (![device hasMediaType:AVMediaTypeVideo] ||
                qf->dev_count >= MAX_DEV_COUNT)
            {
                continue;
            }

            if (front_idx == -1 &&
                [device position] == AVCaptureDevicePositionFront)
            {
                front_idx = qf->dev_count;
            }

            qdi = &qf->dev_info[qf->dev_count++];
            pj_bzero(qdi, sizeof(*qdi));
            pj_ansi_strncpy(qdi->info.name, [device.localizedName UTF8String],
                            sizeof(qdi->info.name));
            pj_ansi_strncpy(qdi->info.driver, "AVF", sizeof(qdi->info.driver));
            qdi->info.dir = PJMEDIA_DIR_CAPTURE;
            qdi->info.has_callback = PJ_FALSE;
#if TARGET_OS_IPHONE
            qdi->info.caps = PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW |
		    	     PJMEDIA_VID_DEV_CAP_SWITCH;
#endif
            qdi->dev = device;
        }
    }
    
    /* Set front camera to be the first input device (as default dev) */
    if (front_idx != -1 && front_idx != first_idx) {
        struct darwin_dev_info tmp_dev_info = qf->dev_info[first_idx];
        qf->dev_info[first_idx] = qf->dev_info[front_idx];
        qf->dev_info[front_idx] = tmp_dev_info;
    }

    /* Set supported formats */
    for (i = 0; i < qf->dev_count; i++) {
	qdi = &qf->dev_info[i];
	qdi->info.caps |= PJMEDIA_VID_DEV_CAP_FORMAT |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION |
                          PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE |
                          PJMEDIA_VID_DEV_CAP_ORIENTATION;
	
	for (l = 0; l < PJ_ARRAY_SIZE(darwin_fmts); l++) {
            pjmedia_format *fmt;
            
            /* Simple renderer UIView only supports BGRA */
            if (qdi->info.dir == PJMEDIA_DIR_RENDER &&
                darwin_fmts[l].pjmedia_format != PJMEDIA_FORMAT_BGRA)
            {
                continue;
            }
            
            if (qdi->info.dir == PJMEDIA_DIR_RENDER) {
                fmt = &qdi->info.fmt[qdi->info.fmt_cnt++];
                pjmedia_format_init_video(fmt,
                                          darwin_fmts[l].pjmedia_format,
                                          DEFAULT_WIDTH,
                                          DEFAULT_HEIGHT,
                                          DEFAULT_FPS, 1);
            } else {
                int m;
                AVCaptureDevice *dev = qdi->dev;
                
                /* Set supported size for capture device */
                for(m = 0;
                    m < PJ_ARRAY_SIZE(darwin_sizes) &&
                    qdi->info.fmt_cnt<PJMEDIA_VID_DEV_INFO_FMT_CNT;
                    m++)
                {
                    if ([dev supportsAVCaptureSessionPreset:
                             darwin_sizes[m].preset_str])
                    {
                        /* Landscape video */
                        fmt = &qdi->info.fmt[qdi->info.fmt_cnt++];
                        pjmedia_format_init_video(fmt,
                		darwin_fmts[l].pjmedia_format,
                                darwin_sizes[m].supported_size_w,
                                darwin_sizes[m].supported_size_h,
                                DEFAULT_FPS, 1);
                        /* Portrait video */
                        fmt = &qdi->info.fmt[qdi->info.fmt_cnt++];
                        pjmedia_format_init_video(fmt,
                        	darwin_fmts[l].pjmedia_format,
                          	darwin_sizes[m].supported_size_h,
                          	darwin_sizes[m].supported_size_w,
                                DEFAULT_FPS, 1);
                    }
                }                
            }
	}
    }
    
    PJ_LOG(4, (THIS_FILE, "Darwin video initialized with %d devices:",
	       qf->dev_count));
    for (i = 0; i < qf->dev_count; i++) {
        qdi = &qf->dev_info[i];
        PJ_LOG(4, (THIS_FILE, "%2d: [%s] %s - %s", i,
                   (qdi->info.dir==PJMEDIA_DIR_CAPTURE? "Capturer":"Renderer"),
                   qdi->info.driver, qdi->info.name));
    }

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned darwin_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;
    return qf->dev_count;
}

/* API: get device info */
static pj_status_t darwin_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					       unsigned index,
					       pjmedia_vid_dev_info *info)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &qf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t darwin_factory_default_param(pj_pool_t *pool,
					        pjmedia_vid_dev_factory *f,
					        unsigned index,
					        pjmedia_vid_dev_param *param)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;
    struct darwin_dev_info *di;

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);
    PJ_UNUSED_ARG(pool);
    
    di = &qf->dev_info[index];

    pj_bzero(param, sizeof(*param));
    if (di->info.dir & PJMEDIA_DIR_CAPTURE) {
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
    
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

    return PJ_SUCCESS;
}

@implementation VOutDelegate
#if TARGET_OS_IPHONE
- (void)update_image
{    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef cgIm = CGImageCreate(stream->size.w, stream->size.h,
                                    8, 32, stream->bytes_per_row, colorSpace,
                                    kCGImageAlphaFirst |
                                    kCGBitmapByteOrder32Little,
                                    stream->render_data_provider, 0,
                                    false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    
    stream->render_view.layer.contents = (__bridge id)(cgIm);
    CGImageRelease(cgIm);

    [pool release];
}
#endif

- (void)session_runtime_error:(NSNotification *)notification
{
    // This function is called from NSNotificationCenter.
    // Make sure the thread is registered.
    if (!pj_thread_is_registered()) {
        pj_thread_t *thread;
        static pj_thread_desc thread_desc;
        pj_bzero(thread_desc, sizeof(pj_thread_desc));
        pj_thread_register("NSNotificationCenter", thread_desc, &thread);
    }

    NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
    PJ_LOG(3, (THIS_FILE, "Capture session runtime error: %s, %s",
    	       [error.localizedDescription UTF8String],
    	       [error.localizedFailureReason UTF8String]));
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput 
		      didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
		      fromConnection:(AVCaptureConnection *)connection
{
    CVImageBufferRef img;
    pj_status_t status;
    void *frame_buf;

    /* Refrain from calling pjlib functions which require thread registration
     * here, since according to the doc, dispatch queue cannot guarantee
     * the reliability of underlying functions required by PJSIP to perform
     * the registration, such as pthread_self() and pthread_getspecific()/
     * pthread_setspecific().
     */

    if (!sampleBuffer)
	return;
    
    /* Get a CMSampleBuffer's Core Video image buffer for the media data */
    img = CMSampleBufferGetImageBuffer(sampleBuffer);
    
    /* Lock the base address of the pixel buffer */
    CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);

    [stream->frame_lock lock];
    stream->has_image = PJ_TRUE;
    
    if (stream->is_planar && stream->capture_buf) {
        if (stream->param.fmt.id == PJMEDIA_FORMAT_I420) {
            /* kCVPixelFormatType_420YpCbCr8BiPlanar* is NV12 */
            pj_uint8_t *p, *p_end, *Y, *U, *V;
            pj_size_t p_len;
            pj_size_t wxh = stream->vid_size.w * stream->vid_size.h;
            /* Image stride is not always equal to the image width.
             * For example, resolution 352*288 can have a stride of 384.
             */
            pj_size_t stride = CVPixelBufferGetBytesPerRowOfPlane(img, 0);
            /* Image height is not always equal to the video resolution.
             * For example, resolution 352*288 can have a height of 264.
             */
            pj_size_t height = CVPixelBufferGetHeight(img);
            pj_bool_t need_clip;
            
            /* Auto detect rotation */
            if ((stream->vid_size.w > stream->vid_size.h && stride < height) ||
                (stream->vid_size.h > stream->vid_size.w && stride > height))
            {
            	pj_size_t w = stream->vid_size.w;
                stream->vid_size.w = stream->vid_size.h;
                stream->vid_size.h = w;
            }
            
            need_clip = (stride != stream->vid_size.w);
            
            p = (pj_uint8_t*)CVPixelBufferGetBaseAddressOfPlane(img, 0);

            p_len = stream->vid_size.w * height;
            Y = (pj_uint8_t*)stream->capture_buf;
            U = Y + wxh;
            V = U + wxh/4;

            if (!need_clip) {
                pj_memcpy(Y, p, p_len);
                Y += p_len;
            } else {
                int i = 0;
                for (; i < height; ++i) {
                    pj_memcpy(Y, p, stream->vid_size.w);
                    Y += stream->vid_size.w;
                    p += stride;
                }
            }
            
            if (stream->vid_size.h > height) {
                pj_memset(Y, 16, (stream->vid_size.h - height) *
                	  stream->vid_size.w);
            }

            p = (pj_uint8_t*)CVPixelBufferGetBaseAddressOfPlane(img, 1);
            if (!need_clip) {
                p_len >>= 1;
                p_end = p + p_len;
                
                while (p < p_end) {
                    *U++ = *p++;
                    *V++ = *p++;
                }
            } else {
                int i = 0;
                for (;i<height/2;++i) {
                    int y=0;
                    for (;y<(stream->vid_size.w)/2;++y) {
                        *U++ = *p++;
                        *V++ = *p++;
                    }
                    p += (stride - stream->vid_size.w);
                }
            }

            if (stream->vid_size.h > height) {
            	pj_size_t UV_size = (stream->vid_size.h - height) *
                	  	    stream->vid_size.w / 4;
                pj_memset(U, 0x80, UV_size);
                pj_memset(V, 0x80, UV_size);
            }
        }
    } else {
        pj_memcpy(stream->capture_buf, CVPixelBufferGetBaseAddress(img),
                  stream->frame_size);
    }
    
    status = pjmedia_vid_dev_conv_resize_and_rotate(&stream->conv, 
    						    stream->capture_buf,
    				       		    &frame_buf);
    if (status == PJ_SUCCESS) {
        stream->frame_buf = frame_buf;
    }

    stream->frame_ts.u64 += stream->ts_inc;
    [stream->frame_lock unlock];
    
    /* Unlock the pixel buffer */
    CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
}
@end

static pj_status_t darwin_stream_get_frame(pjmedia_vid_dev_stream *strm,
                                        pjmedia_frame *frame)
{
    struct darwin_stream *stream = (struct darwin_stream *)strm;

    if (!stream->has_image) {
	frame->size = 0;
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	frame->timestamp.u64 = stream->frame_ts.u64;
	return PJMEDIA_EVID_NOTREADY;
    }

    frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
    frame->bit_info = 0;
    pj_assert(frame->size >= stream->frame_size);
    frame->size = stream->frame_size;
    frame->timestamp.u64 = stream->frame_ts.u64;
    
    [stream->frame_lock lock];
    pj_memcpy(frame->buf, stream->frame_buf, stream->frame_size);
    [stream->frame_lock unlock];
    
    return PJ_SUCCESS;
}


static darwin_fmt_info* get_darwin_format_info(pjmedia_format_id id)
{
    unsigned i;
    
    for (i = 0; i < PJ_ARRAY_SIZE(darwin_fmts); i++) {
        if (darwin_fmts[i].pjmedia_format == id)
            return &darwin_fmts[i];
    }
    
    return NULL;
}


#if TARGET_OS_IPHONE
static pj_status_t darwin_init_view(struct darwin_stream *strm)
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
    
    strm->render_view = [[UIView alloc] initWithFrame:view_rect];
    strm->param.window.info.ios.window = strm->render_view;

    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        PJ_ASSERT_RETURN(param->window.info.ios.window, PJ_EINVAL);
        darwin_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW,
                           param->window.info.ios.window);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE) {
        darwin_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
                           &param->window_hide);
    }
    if (param->flags & PJMEDIA_VID_DEV_CAP_ORIENTATION) {
        darwin_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                           &param->orient);
    }

    return PJ_SUCCESS;
}
#endif /* TARGET_OS_IPHONE */


/* API: create stream */
static pj_status_t darwin_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_dev_param *param,
					const pjmedia_vid_dev_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm)
{
    struct darwin_factory *qf = (struct darwin_factory*)f;
    pj_pool_t *pool;
    struct darwin_stream *strm;
    pjmedia_video_format_detail *vfd;
    const pjmedia_video_format_info *vfi;
    pj_status_t status = PJ_SUCCESS;
    darwin_fmt_info *ifi = get_darwin_format_info(param->fmt.id);

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
		     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO &&
                     (param->dir == PJMEDIA_DIR_CAPTURE ||
                     param->dir == PJMEDIA_DIR_RENDER),
		     PJ_EINVAL);

    if (!(ifi = get_darwin_format_info(param->fmt.id)))
        return PJMEDIA_EVID_BADFORMAT;
    
    vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
    if (!vfi)
        return PJMEDIA_EVID_BADFORMAT;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(qf->pf, "darwin-dev", 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct darwin_stream);
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

    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        int i;
	        
        /* Create capture stream here */
	strm->cap_session = [[AVCaptureSession alloc] init];
	if (!strm->cap_session) {
	    PJ_LOG(2, (THIS_FILE, "Unable to create AV capture session"));
	    status = PJ_ENOMEM;
	    goto on_error;
	}
        AVCaptureDevice *dev = qf->dev_info[param->cap_id].dev;
 
        for (i = PJ_ARRAY_SIZE(darwin_sizes)-1; i > 0; --i) {
            if (((vfd->size.w == darwin_sizes[i].supported_size_w) &&
                 (vfd->size.h == darwin_sizes[i].supported_size_h)) ||
                ((vfd->size.w == darwin_sizes[i].supported_size_h) &&
                 (vfd->size.h == darwin_sizes[i].supported_size_w)))
            {
                break;
            }
        }
        
        strm->cap_session.sessionPreset = darwin_sizes[i].preset_str;
        
        /* If the requested size is portrait (or landscape), we make
         * our natural orientation portrait (or landscape) as well.
         */
        if (vfd->size.w > vfd->size.h) {
            vfd->size.w = darwin_sizes[i].supported_size_w;
            vfd->size.h = darwin_sizes[i].supported_size_h;
        } else {
            vfd->size.h = darwin_sizes[i].supported_size_w;
            vfd->size.w = darwin_sizes[i].supported_size_h;
        }
        strm->size = vfd->size;
        strm->vid_size = vfd->size;
        strm->bytes_per_row = strm->size.w * vfi->bpp / 8;
        strm->frame_size = strm->bytes_per_row * strm->size.h;
        
        /* Update param as output */
        param->fmt = strm->param.fmt;

#if TARGET_OS_IPHONE
        /* Set frame rate, this may only work on iOS 7 or later.
         * On Mac, this may raise an exception if we set it to a value
         * unsupported by the device. We need to query
         * activeFormat.videoSupportedFrameRateRanges to get the valid
         * range.
         */
        if ([dev respondsToSelector:@selector(activeVideoMinFrameDuration)] &&
            [dev lockForConfiguration:NULL])
        {
            dev.activeVideoMinFrameDuration = CMTimeMake(vfd->fps.denum,
                                                         vfd->fps.num);
            dev.activeVideoMaxFrameDuration = CMTimeMake(vfd->fps.denum,
                                                         vfd->fps.num);
            [dev unlockForConfiguration];
        }
#endif
        
	/* Add the video device to the session as a device input */
        NSError *error;
	strm->dev_input = [AVCaptureDeviceInput
			   deviceInputWithDevice:dev
			   error: &error];
	if (error || !strm->dev_input) {
	    PJ_LOG(2, (THIS_FILE, "Unable to get input capture device"));
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	if ([strm->cap_session canAddInput:strm->dev_input]) {
	    [strm->cap_session addInput:strm->dev_input];
	} else {
	    PJ_LOG(2, (THIS_FILE, "Unable to add input capture device"));
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	strm->video_output = [[AVCaptureVideoDataOutput alloc] init];
	if (!strm->video_output) {
	    PJ_LOG(2, (THIS_FILE, "Unable to create AV video output"));
	    status = PJ_ENOMEM;
	    goto on_error;
	}
        
	/* Configure the video output */
        strm->video_output.alwaysDiscardsLateVideoFrames = YES;
	strm->video_output.videoSettings =
	    [NSDictionary dictionaryWithObjectsAndKeys:
#if !TARGET_OS_IPHONE
    			  /* On Mac, we need to set the buffer's dimension
    			   * to avoid extra padding.
    			   */
                          [NSNumber numberWithDouble:
                          	    darwin_sizes[i].supported_size_w],
                          (id)kCVPixelBufferWidthKey,
                          [NSNumber numberWithDouble:
                          	    darwin_sizes[i].supported_size_h],
                          (id)kCVPixelBufferHeightKey,
#endif
			  [NSNumber numberWithInt:ifi->darwin_format],
			  kCVPixelBufferPixelFormatTypeKey, nil];

	strm->vout_delegate = [VOutDelegate alloc];
	strm->vout_delegate->stream = strm;
	strm->queue = dispatch_queue_create("vout_queue",
					    DISPATCH_QUEUE_SERIAL);
	[strm->video_output setSampleBufferDelegate:strm->vout_delegate
                            queue:strm->queue];

	/* Add observer to catch notification when the capture session
	 * fails to start running or encounters an error during runtime.
	 */
	[[NSNotificationCenter defaultCenter] addObserver:strm->vout_delegate
	    selector:@selector(session_runtime_error:)
	    name:AVCaptureSessionRuntimeErrorNotification
	    object:strm->cap_session];
        
        if ([strm->cap_session canAddOutput:strm->video_output]) {
	    [strm->cap_session addOutput:strm->video_output];
	} else {
	    PJ_LOG(2, (THIS_FILE, "Unable to add video data output"));
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	strm->capture_buf = pj_pool_alloc(strm->pool, strm->frame_size);
	strm->frame_buf = strm->capture_buf;
	strm->frame_lock = [[NSLock alloc]init];
	
        /* Native preview */
        if (param->flags & PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW) {
            darwin_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW,
                               &param->native_preview);
        }

        /* Video orientation.
         * If we send in portrait, we need to set up orientation converter
         * as well.
         */
        if ((param->flags & PJMEDIA_VID_DEV_CAP_ORIENTATION) ||
            (vfd->size.h > vfd->size.w))
        {
            if (param->orient == PJMEDIA_ORIENT_UNKNOWN)
                param->orient = PJMEDIA_ORIENT_NATURAL;
            darwin_stream_set_cap(&strm->base, PJMEDIA_VID_DEV_CAP_ORIENTATION,
                               &param->orient);
        }
        
    } else if (param->dir & PJMEDIA_DIR_RENDER) {
#if TARGET_OS_IPHONE
        /* Create renderer stream here */
        
        dispatch_sync_on_main_queue(^{
            darwin_init_view(strm);
        });
        
	if (!strm->vout_delegate) {
	    strm->vout_delegate = [VOutDelegate alloc];
	    strm->vout_delegate->stream = strm;
	}
        
	strm->render_buf = pj_pool_alloc(pool, strm->frame_size);
	strm->render_buf_size = strm->frame_size;
        strm->render_data_provider = CGDataProviderCreateWithData(NULL,
                                            strm->render_buf, strm->frame_size,
                                            NULL);
#endif
    }
    
    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;
    
    return PJ_SUCCESS;
    
on_error:
    darwin_stream_destroy((pjmedia_vid_dev_stream *)strm);
    
    return status;
}

/* API: Get stream info. */
static pj_status_t darwin_stream_get_param(pjmedia_vid_dev_stream *s,
				           pjmedia_vid_dev_param *pi)
{
    struct darwin_stream *strm = (struct darwin_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t darwin_stream_get_cap(pjmedia_vid_dev_stream *s,
				         pjmedia_vid_dev_cap cap,
				         void *pval)
{
    struct darwin_stream *strm = (struct darwin_stream*)s;
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    PJ_UNUSED_ARG(strm);

    switch (cap) {
#if TARGET_OS_IPHONE
        case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
        {
            pjmedia_vid_dev_hwnd *hwnd = (pjmedia_vid_dev_hwnd*) pval;
            hwnd->type = PJMEDIA_VID_DEV_HWND_TYPE_NONE;
            hwnd->info.ios.window = (void*)strm->render_view;
            return PJ_SUCCESS;
        }
#endif /* TARGET_OS_IPHONE */
        default:
            break;
    }
    
    return PJMEDIA_EVID_INVCAP;
}

/* API: set capability */
static pj_status_t darwin_stream_set_cap(pjmedia_vid_dev_stream *s,
				         pjmedia_vid_dev_cap cap,
				         const void *pval)
{
    struct darwin_stream *strm = (struct darwin_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    switch (cap) {
        /* Native preview */
        case PJMEDIA_VID_DEV_CAP_INPUT_PREVIEW:
        {
            pj_bool_t native_preview = *((pj_bool_t *)pval);
            
	    /* Disable native preview */
            if (!native_preview) {
		if (strm->prev_layer) {
		    CALayer *prev_layer = strm->prev_layer;
                    dispatch_sync_on_main_queue(^{
		        [prev_layer removeFromSuperlayer];
		        [prev_layer release];
		    });
		    strm->prev_layer = nil;
	            PJ_LOG(4, (THIS_FILE, "Native preview deinitialized"));
		}
                return PJ_SUCCESS;
            }
            
	    /* Enable native preview */

	    /* Verify if it is already enabled */
	    if (strm->prev_layer)
	        return PJ_SUCCESS;
	    
	    /* Verify capture session instance availability */
            if (!strm->cap_session)
		return PJ_EINVALIDOP;
            
#if TARGET_OS_IPHONE
            /* Preview layer instantiation should be in main thread! */
            dispatch_sync_on_main_queue(^{
            	/* Create view, if none */
	    	if (!strm->render_view)
	            darwin_init_view(strm);
            
                /* Create preview layer */
                AVCaptureVideoPreviewLayer *prev_layer =
                            [[AVCaptureVideoPreviewLayer alloc]
                             initWithSession:strm->cap_session];
                
                /* Attach preview layer to a UIView */
                prev_layer.videoGravity = AVLayerVideoGravityResize;
                prev_layer.frame = strm->render_view.bounds;
                [strm->render_view.layer addSublayer:prev_layer];
                strm->prev_layer = prev_layer;
            });
            PJ_LOG(4, (THIS_FILE, "Native preview initialized"));
#endif
            
            return PJ_SUCCESS;
        }

        /* Fast switch */
        case PJMEDIA_VID_DEV_CAP_SWITCH:
        {
            if (!strm->cap_session) return PJ_EINVAL;
            
            NSError *error;
            struct darwin_dev_info* di = strm->factory->dev_info;
            pjmedia_vid_dev_switch_param *p =
                                    (pjmedia_vid_dev_switch_param*)pval;

            /* Verify target capture ID */
            if (p->target_id < 0 || p->target_id >= strm->factory->dev_count)
                return PJ_EINVAL;
            
            if (di[p->target_id].info.dir != PJMEDIA_DIR_CAPTURE ||
                !di[p->target_id].dev)
            {
                return PJ_EINVAL;
            }
            
            /* Just return if current and target device are the same */
            if (strm->param.cap_id == p->target_id)
                return PJ_SUCCESS;
            
            /* Ok, let's do the switch */
            AVCaptureDeviceInput *cur_dev_input = strm->dev_input;
            AVCaptureDeviceInput *new_dev_input =
                    [AVCaptureDeviceInput
                     deviceInputWithDevice:di[p->target_id].dev
                     error:&error];

            [strm->cap_session beginConfiguration];
            [strm->cap_session removeInput:cur_dev_input];
            [strm->cap_session addInput:new_dev_input];
            [strm->cap_session commitConfiguration];
            
            strm->dev_input = new_dev_input;
            strm->param.cap_id = p->target_id;
            
            /* Set the orientation as well */
            darwin_stream_set_cap(s, PJMEDIA_VID_DEV_CAP_ORIENTATION,
            		       &strm->param.orient);
            
            return PJ_SUCCESS;
        }
        
#if TARGET_OS_IPHONE
        case PJMEDIA_VID_DEV_CAP_FORMAT:
	{
            const pjmedia_video_format_info *vfi;
            pjmedia_video_format_detail *vfd;
            pjmedia_format *fmt = (pjmedia_format *)pval;
            darwin_fmt_info *ifi;
        
            if (!(ifi = get_darwin_format_info(fmt->id)))
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
		CGDataProviderRelease(strm->render_data_provider);
	        strm->render_data_provider = CGDataProviderCreateWithData(NULL,
	                                                strm->render_buf,
	                                                strm->frame_size,
	                                                NULL);
	    }
	    
	    return PJ_SUCCESS;
	}
	
        case PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW:
        {
            UIView *view = (UIView *)pval;
            strm->param.window.info.ios.window = (void *)pval;
            dispatch_sync_on_main_queue(^{
                [view addSubview:strm->render_view];
            });
            return PJ_SUCCESS;
        }
            
        case PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE:
        {
            pj_memcpy(&strm->param.disp_size, pval,
                      sizeof(strm->param.disp_size));
            dispatch_sync_on_main_queue(^{
            	CGRect r = strm->render_view.bounds;
            	r.size = CGSizeMake(strm->param.disp_size.w,
                                    strm->param.disp_size.h);
		strm->render_view.bounds = r;
                if (strm->prev_layer)
                    strm->prev_layer.frame = r;
            });
            return PJ_SUCCESS;
        }
    
        case PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION:
        {
            pj_memcpy(&strm->param.window_pos, pval,
                      sizeof(strm->param.window_pos));
            dispatch_sync_on_main_queue(^{
                strm->render_view.center =
                            CGPointMake(strm->param.window_pos.x +
                                        strm->param.disp_size.w/2.0,
                                        strm->param.window_pos.y +
                                        strm->param.disp_size.h/2.0);
            });
            return PJ_SUCCESS;
        }
            
        case PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE:
        {
            dispatch_sync_on_main_queue(^{
                strm->render_view.hidden = (BOOL)(*((pj_bool_t *)pval));
            });
            return PJ_SUCCESS;
        }
#endif /* TARGET_OS_IPHONE */
        
        case PJMEDIA_VID_DEV_CAP_ORIENTATION:
        {
            pjmedia_orient orient = *(pjmedia_orient *)pval;

	    pj_assert(orient >= PJMEDIA_ORIENT_UNKNOWN &&
	              orient <= PJMEDIA_ORIENT_ROTATE_270DEG);

            if (orient == PJMEDIA_ORIENT_UNKNOWN)
                return PJ_EINVAL;

            pj_memcpy(&strm->param.orient, pval,
                      sizeof(strm->param.orient));
        
            if (strm->param.dir == PJMEDIA_DIR_RENDER) {
#if TARGET_OS_IPHONE
                dispatch_sync_on_main_queue(^{
                    strm->render_view.transform =
                        CGAffineTransformMakeRotation(
                            ((int)strm->param.orient-1) * -M_PI_2);
                });
#endif

		return PJ_SUCCESS;
            }
        
            const AVCaptureVideoOrientation cap_ori[4] =
            {
   		AVCaptureVideoOrientationLandscapeLeft,      /* NATURAL */
        	AVCaptureVideoOrientationPortrait,           /* 90DEG   */
   		AVCaptureVideoOrientationLandscapeRight,     /* 180DEG  */
   		AVCaptureVideoOrientationPortraitUpsideDown, /* 270DEG  */
            };
	    AVCaptureConnection *vidcon;
	    pj_bool_t support_ori = PJ_TRUE;
	    
	    pj_assert(strm->param.dir == PJMEDIA_DIR_CAPTURE);
	    
	    if (!strm->video_output)
	        return PJMEDIA_EVID_NOTREADY;

	    vidcon = [strm->video_output 
	              connectionWithMediaType:AVMediaTypeVideo];
	    if ([vidcon isVideoOrientationSupported]) {
	        vidcon.videoOrientation = cap_ori[strm->param.orient-1];
	    } else {
	        support_ori = PJ_FALSE;
	    }
	    
	    if (!strm->conv.conv) {
	        pj_status_t status;
	        pjmedia_rect_size orig_size;

	        /* Original native size of device is landscape */
	        orig_size.w = (strm->size.w > strm->size.h? strm->size.w :
	        	       strm->size.h);
	        orig_size.h = (strm->size.w > strm->size.h? strm->size.h :
	        	       strm->size.w);

		if (!support_ori) {
	            PJ_LOG(4, (THIS_FILE, "Native video capture orientation " 
	        		          "unsupported, will use converter's "
	        		          "rotation."));
	        }

	        status = pjmedia_vid_dev_conv_create_converter(
	        				 &strm->conv, strm->pool,
	        		        	 &strm->param.fmt,
	        		        	 orig_size, strm->size,
	        		        	 (support_ori?PJ_FALSE:PJ_TRUE),
	        		        	 MAINTAIN_ASPECT_RATIO);
	    	
	    	if (status != PJ_SUCCESS)
	    	    return status;
	    }
	    
	    pjmedia_vid_dev_conv_set_rotation(&strm->conv, strm->param.orient);
	    
	    PJ_LOG(5, (THIS_FILE, "Video capture orientation set to %d",
	    			  strm->param.orient));

            return PJ_SUCCESS;
        }
        
        default:
            break;
    }

    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t darwin_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct darwin_stream *stream = (struct darwin_stream*)strm;

    PJ_UNUSED_ARG(stream);

    PJ_LOG(4, (THIS_FILE, "Starting Darwin video stream"));

    if (stream->cap_session) {
        dispatch_sync_on_main_queue(^{
            [stream->cap_session startRunning];
        });
    
	if (![stream->cap_session isRunning]) {
	    /* More info about the error should be reported in
	     * VOutDelegate::session_runtime_error()
	     */
	    PJ_LOG(3, (THIS_FILE, "Unable to start AVFoundation capture "
				  "session"));
	    return PJ_EUNKNOWN;
	}
    }
    
    return PJ_SUCCESS;
}


/* API: Put frame from stream */
static pj_status_t darwin_stream_put_frame(pjmedia_vid_dev_stream *strm,
					   const pjmedia_frame *frame)
{
#if TARGET_OS_IPHONE
    struct darwin_stream *stream = (struct darwin_stream*)strm;

    /* Video conference just trying to send heart beat for updating timestamp
     * or keep-alive, this port doesn't need any, just ignore.
     */
    if (frame->size==0 || frame->buf==NULL)
	return PJ_SUCCESS;
	
    if (stream->frame_size >= frame->size)
        pj_memcpy(stream->render_buf, frame->buf, frame->size);
    else
        pj_memcpy(stream->render_buf, frame->buf, stream->frame_size);
    
    /* Perform video display in a background thread */
    dispatch_sync_on_main_queue(^{
        [stream->vout_delegate update_image];
    });
#endif

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t darwin_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct darwin_stream *stream = (struct darwin_stream*)strm;

    if (!stream->cap_session || ![stream->cap_session isRunning])
        return PJ_SUCCESS;
    
    PJ_LOG(4, (THIS_FILE, "Stopping Darwin video stream"));

    dispatch_sync_on_main_queue(^{
        [stream->cap_session stopRunning];
    });
    stream->has_image = PJ_FALSE;
    
    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t darwin_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct darwin_stream *stream = (struct darwin_stream*)strm;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    darwin_stream_stop(strm);
    
    if (stream->cap_session) {
        if (stream->dev_input) {
            [stream->cap_session removeInput:stream->dev_input];
            stream->dev_input = nil;
        }
        [stream->cap_session removeOutput:stream->video_output];
	[stream->cap_session release];
	stream->cap_session = nil;
    }
 
    if (stream->video_output) {
        [stream->video_output release];
        stream->video_output = nil;
    }

    if (stream->vout_delegate) {
	[stream->vout_delegate release];
	stream->vout_delegate = nil;
    }

#if TARGET_OS_IPHONE
    if (stream->prev_layer) {
        CALayer *prev_layer = stream->prev_layer;
        dispatch_sync_on_main_queue(^{
            [prev_layer removeFromSuperlayer];
            [prev_layer release];
        });
        stream->prev_layer = nil;
    }
    
    if (stream->render_view) {
        UIView *view = stream->render_view;
        dispatch_sync_on_main_queue(^{
            [view removeFromSuperview];
            [view release];
        });
        stream->render_view = nil;
    }
    
    if (stream->render_data_provider) {
        CGDataProviderRelease(stream->render_data_provider);
        stream->render_data_provider = nil;
    }
#endif /* TARGET_OS_IPHONE */

    if (stream->queue) {
        dispatch_release(stream->queue);
        stream->queue = nil;
    }
    
    if (stream->frame_lock) {
        [stream->frame_lock release];
        stream->frame_lock = nil;
    }
    
    pjmedia_vid_dev_conv_destroy_converter(&stream->conv);

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_DARWIN */

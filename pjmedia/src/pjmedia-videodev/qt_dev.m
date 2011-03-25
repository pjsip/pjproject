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
#include <pjmedia-videodev/videodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if PJMEDIA_VIDEO_DEV_HAS_QT

#include <QTKit/QTKit.h>

#define THIS_FILE		"qt_dev.c"
#define DEFAULT_CLOCK_RATE	9000
#define DEFAULT_WIDTH		640
#define DEFAULT_HEIGHT		480
#define DEFAULT_FPS		15

#define kCVPixelFormatType_422YpCbCr8_yuvs 'yuvs'

typedef struct qt_fmt_info
{
    pjmedia_format_id   pjmedia_format;
    unsigned		qt_format;
} qt_fmt_info;

static qt_fmt_info qt_fmts[] =
{
    {PJMEDIA_FORMAT_YUY2, kCVPixelFormatType_422YpCbCr8_yuvs} ,
};

/* qt device info */
struct qt_dev_info
{
    pjmedia_vid_dev_info	 info;
    char			 dev_id[192];
};

/* qt factory */
struct qt_factory
{
    pjmedia_vid_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;

    unsigned			 dev_count;
    struct qt_dev_info		*dev_info;
};

@interface VOutDelegate: NSObject
{
@public
    struct qt_stream *stream;
}
@end

/* Video stream. */
struct qt_stream
{
    pjmedia_vid_dev_stream  base;	    /**< Base stream	       */
    pjmedia_vid_param	    param;	    /**< Settings	       */
    pj_pool_t		   *pool;           /**< Memory pool.          */

    pj_timestamp	    cap_frame_ts;   /**< Captured frame tstamp */
    unsigned		    cap_ts_inc;	    /**< Increment	       */
    
    pjmedia_vid_cb	    vid_cb;         /**< Stream callback.      */
    void		   *user_data;      /**< Application data.     */

    QTCaptureSession			*cap_session;
    QTCaptureDeviceInput		*dev_input;
    QTCaptureDecompressedVideoOutput	*video_output;
    VOutDelegate			*vout_delegate;
};


/* Prototypes */
static pj_status_t qt_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t qt_factory_destroy(pjmedia_vid_dev_factory *f);
static unsigned    qt_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t qt_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					   unsigned index,
					   pjmedia_vid_dev_info *info);
static pj_status_t qt_factory_default_param(pj_pool_t *pool,
					    pjmedia_vid_dev_factory *f,
					    unsigned index,
					    pjmedia_vid_param *param);
static pj_status_t qt_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_param *param,
					const pjmedia_vid_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm);

static pj_status_t qt_stream_get_param(pjmedia_vid_dev_stream *strm,
				       pjmedia_vid_param *param);
static pj_status_t qt_stream_get_cap(pjmedia_vid_dev_stream *strm,
				     pjmedia_vid_dev_cap cap,
				     void *value);
static pj_status_t qt_stream_set_cap(pjmedia_vid_dev_stream *strm,
				     pjmedia_vid_dev_cap cap,
				     const void *value);
static pj_status_t qt_stream_start(pjmedia_vid_dev_stream *strm);
static pj_status_t qt_stream_stop(pjmedia_vid_dev_stream *strm);
static pj_status_t qt_stream_destroy(pjmedia_vid_dev_stream *strm);

/* Operations */
static pjmedia_vid_dev_factory_op factory_op =
{
    &qt_factory_init,
    &qt_factory_destroy,
    &qt_factory_get_dev_count,
    &qt_factory_get_dev_info,
    &qt_factory_default_param,
    &qt_factory_create_stream
};

static pjmedia_vid_dev_stream_op stream_op =
{
    &qt_stream_get_param,
    &qt_stream_get_cap,
    &qt_stream_set_cap,
    &qt_stream_start,
    NULL,
    NULL,
    &qt_stream_stop,
    &qt_stream_destroy
};


/****************************************************************************
 * Factory operations
 */
/*
 * Init qt_ video driver.
 */
pjmedia_vid_dev_factory* pjmedia_qt_factory(pj_pool_factory *pf)
{
    struct qt_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "qt video", 4000, 4000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct qt_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t qt_factory_init(pjmedia_vid_dev_factory *f)
{
    struct qt_factory *qf = (struct qt_factory*)f;
    struct qt_dev_info *qdi;
    unsigned i, dev_count = 0;
    NSArray *dev_array;

    dev_array = [QTCaptureDevice inputDevices];
    for (i = 0; i < [dev_array count]; i++) {
	QTCaptureDevice *dev = [dev_array objectAtIndex:i];
	if ([dev hasMediaType:QTMediaTypeVideo] ||
	    [dev hasMediaType:QTMediaTypeMuxed])
	{
	    dev_count++;
	}
    }
    
    /* Initialize input and output devices here */
    qf->dev_count = 0;
    qf->dev_info = (struct qt_dev_info*)
		   pj_pool_calloc(qf->pool, dev_count,
				  sizeof(struct qt_dev_info));
    for (i = 0; i < [dev_array count]; i++) {
	QTCaptureDevice *dev = [dev_array objectAtIndex:i];
	if ([dev hasMediaType:QTMediaTypeVideo] ||
	    [dev hasMediaType:QTMediaTypeMuxed])
	{
	    unsigned j, k;
	    
	    qdi = &qf->dev_info[qf->dev_count++];
	    pj_bzero(qdi, sizeof(*qdi));
	    [[dev localizedDisplayName] getCString:qdi->info.name
					maxLength:sizeof(qdi->info.name)
					encoding:
					[NSString defaultCStringEncoding]];
	    [[dev uniqueID] getCString:qdi->dev_id
			    maxLength:sizeof(qdi->dev_id)
			    encoding:[NSString defaultCStringEncoding]];
	    strcpy(qdi->info.driver, "QT");	    
	    qdi->info.dir = PJMEDIA_DIR_CAPTURE;
	    qdi->info.has_callback = PJ_TRUE;

	    qdi->info.fmt_cnt = 0;
	    for (k = 0; k < [[dev formatDescriptions] count]; k++) {
		QTFormatDescription *desc = [[dev formatDescriptions]
					     objectAtIndex:k];
		for (j = 0; j < PJ_ARRAY_SIZE(qt_fmts); j++) {
		    if ([desc formatType] == qt_fmts[j].qt_format) {
			qdi->info.fmt_cnt++;
			break;
		    }
		}
	    }
	    
	    qdi->info.caps = PJMEDIA_VID_DEV_CAP_FORMAT;
	    qdi->info.fmt = (pjmedia_format*)
			    pj_pool_calloc(qf->pool, qdi->info.fmt_cnt,
					   sizeof(pjmedia_format));
	    for (j = k = 0; k < [[dev formatDescriptions] count]; k++) {
		unsigned l;
		QTFormatDescription *desc = [[dev formatDescriptions]
					     objectAtIndex:k];
		for (l = 0; l < PJ_ARRAY_SIZE(qt_fmts); l++) {
		    if ([desc formatType] == qt_fmts[j].qt_format) {
			pjmedia_format *fmt = &qdi->info.fmt[j++];
			pjmedia_format_init_video(fmt,
						  qt_fmts[l].pjmedia_format,
						  DEFAULT_WIDTH,
						  DEFAULT_HEIGHT,
						  DEFAULT_FPS, 1);
			break;
		    }
		}
	    }

	    PJ_LOG(4, (THIS_FILE, " dev_id %d: %s", i, qdi->info.name));    
	}
    }

    PJ_LOG(4, (THIS_FILE, "qt video initialized with %d devices",
	       qf->dev_count));
    
    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t qt_factory_destroy(pjmedia_vid_dev_factory *f)
{
    struct qt_factory *qf = (struct qt_factory*)f;
    pj_pool_t *pool = qf->pool;

    qf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned qt_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    struct qt_factory *qf = (struct qt_factory*)f;
    return qf->dev_count;
}

/* API: get device info */
static pj_status_t qt_factory_get_dev_info(pjmedia_vid_dev_factory *f,
					   unsigned index,
					   pjmedia_vid_dev_info *info)
{
    struct qt_factory *qf = (struct qt_factory*)f;

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);

    pj_memcpy(info, &qf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t qt_factory_default_param(pj_pool_t *pool,
					    pjmedia_vid_dev_factory *f,
					    unsigned index,
					    pjmedia_vid_param *param)
{
    struct qt_factory *qf = (struct qt_factory*)f;
    struct qt_dev_info *di = &qf->dev_info[index];

    PJ_ASSERT_RETURN(index < qf->dev_count, PJMEDIA_EVID_INVDEV);

    PJ_UNUSED_ARG(pool);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE;
    param->cap_id = index;
    param->rend_id = PJMEDIA_VID_INVALID_DEV;
    param->flags = PJMEDIA_VID_DEV_CAP_FORMAT;
    param->clock_rate = DEFAULT_CLOCK_RATE;
    pj_memcpy(&param->fmt, &di->info.fmt[0], sizeof(param->fmt));

    return PJ_SUCCESS;
}

@implementation VOutDelegate
- (void)captureOutput:(QTCaptureOutput *)captureOutput
		      didOutputVideoFrame:(CVImageBufferRef)videoFrame
		      withSampleBuffer:(QTSampleBuffer *)sampleBuffer
		      fromConnection:(QTCaptureConnection *)connection
{
    unsigned size = [sampleBuffer lengthForAllSamples];
    pjmedia_frame frame;

    if (!videoFrame)
	return;
    
    frame.type = PJMEDIA_TYPE_VIDEO;
    frame.buf = [sampleBuffer bytesForAllSamples];
    frame.size = size;
    frame.bit_info = 0;
    frame.timestamp.u64 = stream->cap_frame_ts.u64;
    
    if (stream->vid_cb.capture_cb)
        (*stream->vid_cb.capture_cb)(&stream->base, stream->user_data,
				     &frame);
    
    stream->cap_frame_ts.u64 += stream->cap_ts_inc;
}
@end

static qt_fmt_info* get_qt_format_info(pjmedia_format_id id)
{
    unsigned i;
    
    for (i = 0; i < PJ_ARRAY_SIZE(qt_fmts); i++) {
        if (qt_fmts[i].pjmedia_format == id)
            return &qt_fmts[i];
    }
    
    return NULL;
}

/* API: create stream */
static pj_status_t qt_factory_create_stream(
					pjmedia_vid_dev_factory *f,
					pjmedia_vid_param *param,
					const pjmedia_vid_cb *cb,
					void *user_data,
					pjmedia_vid_dev_stream **p_vid_strm)
{
    struct qt_factory *qf = (struct qt_factory*)f;
    pj_pool_t *pool;
    struct qt_stream *strm;
    const pjmedia_video_format_info *vfi;
    pj_status_t status = PJ_SUCCESS;
    BOOL success = NO;
    NSError *error;    

    PJ_ASSERT_RETURN(f && param && p_vid_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->fmt.type == PJMEDIA_TYPE_VIDEO &&
		     param->fmt.detail_type == PJMEDIA_FORMAT_DETAIL_VIDEO,
		     PJ_EINVAL);

    vfi = pjmedia_get_video_format_info(NULL, param->fmt.id);
    if (!vfi)
        return PJMEDIA_EVID_BADFORMAT;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(qf->pf, "qt-dev", 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct qt_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    pj_memcpy(&strm->vid_cb, cb, sizeof(*cb));
    strm->user_data = user_data;
    
    /* Create player stream here */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
    }
    
    /* Create capture stream here */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
	const pjmedia_video_format_detail *vfd;
	qt_fmt_info *qfi = get_qt_format_info(param->fmt.id);
	
	if (!qfi) {
	    status = PJMEDIA_EVID_BADFORMAT;
	    goto on_error;
	}

	strm->cap_session = [[QTCaptureSession alloc] init];
	if (!strm->cap_session) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}
	
	/* Open video device */
	QTCaptureDevice *videoDevice = 
	    [QTCaptureDevice deviceWithUniqueID:
			     [NSString stringWithCString:
				       qf->dev_info[param->cap_id].dev_id
				       encoding:
				       [NSString defaultCStringEncoding]]];
	if (!videoDevice || ![videoDevice open:&error]) {
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	/* Add the video device to the session as a device input */	
	strm->dev_input = [[QTCaptureDeviceInput alloc] 
			   initWithDevice:videoDevice];
	success = [strm->cap_session addInput:strm->dev_input error:&error];
	if (!success) {
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	strm->video_output = [[QTCaptureDecompressedVideoOutput alloc] init];
	success = [strm->cap_session addOutput:strm->video_output
				     error:&error];
	if (!success) {
	    status = PJMEDIA_EVID_SYSERR;
	    goto on_error;
	}
	
	vfd = pjmedia_format_get_video_format_detail(&strm->param.fmt,
						     PJ_TRUE);
	[strm->video_output setPixelBufferAttributes:
			    [NSDictionary dictionaryWithObjectsAndKeys:
					  [NSNumber numberWithInt:
						    qfi->qt_format],
					  kCVPixelBufferPixelFormatTypeKey,
					  [NSNumber numberWithInt:
						    vfd->size.w],
					  kCVPixelBufferWidthKey,
					  [NSNumber numberWithInt:
						    vfd->size.h],
					  kCVPixelBufferHeightKey, nil]];

	pj_assert(vfd->fps.num);
	strm->cap_ts_inc = PJMEDIA_SPF2(strm->param.clock_rate, &vfd->fps, 1);
	
	[strm->video_output setMinimumVideoFrameInterval:
			    (1.0f * vfd->fps.denum / (double)vfd->fps.num)];
	
	strm->vout_delegate = [VOutDelegate alloc];
	strm->vout_delegate->stream = strm;
	[strm->video_output setDelegate:strm->vout_delegate];
    }
    
    /* Apply the remaining settings */
    /*    
     if (param->flags & PJMEDIA_VID_DEV_CAP_INPUT_SCALE) {
	qt_stream_set_cap(&strm->base,
			  PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
			  &param->fmt);
     }
     */
    /* Done */
    strm->base.op = &stream_op;
    *p_vid_strm = &strm->base;
    
    return PJ_SUCCESS;
    
on_error:
    qt_stream_destroy((pjmedia_vid_dev_stream *)strm);
    
    return status;
}

/* API: Get stream info. */
static pj_status_t qt_stream_get_param(pjmedia_vid_dev_stream *s,
				       pjmedia_vid_param *pi)
{
    struct qt_stream *strm = (struct qt_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

/*    if (qt_stream_get_cap(s, PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
                            &pi->fmt.info_size) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_VID_DEV_CAP_INPUT_SCALE;
    }
*/
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t qt_stream_get_cap(pjmedia_vid_dev_stream *s,
				     pjmedia_vid_dev_cap cap,
				     void *pval)
{
    struct qt_stream *strm = (struct qt_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE)
    {
        return PJMEDIA_EVID_INVCAP;
//	return PJ_SUCCESS;
    } else {
	return PJMEDIA_EVID_INVCAP;
    }
}

/* API: set capability */
static pj_status_t qt_stream_set_cap(pjmedia_vid_dev_stream *s,
				     pjmedia_vid_dev_cap cap,
				     const void *pval)
{
    struct qt_stream *strm = (struct qt_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_VID_DEV_CAP_INPUT_SCALE)
    {
	return PJ_SUCCESS;
    }

    return PJMEDIA_EVID_INVCAP;
}

/* API: Start stream. */
static pj_status_t qt_stream_start(pjmedia_vid_dev_stream *strm)
{
    struct qt_stream *stream = (struct qt_stream*)strm;

    PJ_UNUSED_ARG(stream);

    PJ_LOG(4, (THIS_FILE, "Starting qt video stream"));

    if (stream->cap_session) {
	[stream->cap_session startRunning];
    
	if (![stream->cap_session isRunning])
	    return PJ_EUNKNOWN;
    }
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    
    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t qt_stream_stop(pjmedia_vid_dev_stream *strm)
{
    struct qt_stream *stream = (struct qt_stream*)strm;

    PJ_UNUSED_ARG(stream);

    PJ_LOG(4, (THIS_FILE, "Stopping qt video stream"));

    if (stream->cap_session && [stream->cap_session isRunning])
	[stream->cap_session stopRunning];
    
    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t qt_stream_destroy(pjmedia_vid_dev_stream *strm)
{
    struct qt_stream *stream = (struct qt_stream*)strm;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    qt_stream_stop(strm);
    
    if (stream->dev_input && [[stream->dev_input device] isOpen])
	[[stream->dev_input device] close];
    
    if (stream->cap_session) {
	[stream->cap_session release];
	stream->cap_session = NULL;
    }
    if (stream->dev_input) {
	[stream->dev_input release];
	stream->dev_input = NULL;
    }
    if (stream->vout_delegate) {
	[stream->vout_delegate release];
	stream->vout_delegate = NULL;
    }
    if (stream->video_output) {
	[stream->video_output release];
	stream->video_output = NULL;
    }

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_VIDEO_DEV_HAS_QT */

/* $Id$ */
/*
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "test.h"
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia-codec/ffmpeg_codecs.h>
#include <pjmedia/vid_codec.h>
#include <pjmedia_videodev.h>

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
#    include "TargetConditionals.h"
#    if !TARGET_OS_IPHONE
#	define VID_DEV_TEST_MAC_OS 1
#    endif
#endif

#if VID_DEV_TEST_MAC_OS
#   include <Foundation/NSAutoreleasePool.h>
#   include <AppKit/NSApplication.h>
#endif

#define THIS_FILE "vid_dev_test.c"

pj_status_t pjmedia_libswscale_converter_init(pjmedia_converter_mgr *mgr,
				              pj_pool_t *pool);

typedef struct codec_port_data_t
{
    pjmedia_vid_codec   *codec;
    pjmedia_port        *src_port;
    pj_uint8_t          *enc_buf;
    pj_size_t            enc_buf_size;

    pjmedia_converter   *conv;
} codec_port_data_t;

typedef struct avi_port_t
{
    pjmedia_vid_port   *vid_port;
    pjmedia_aud_stream *aud_stream;
    pj_bool_t           is_running;
} avi_port_t;

static int enum_devs(void)
{
    unsigned i, dev_cnt;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  device enums"));
    dev_cnt = pjmedia_vid_dev_count();
    for (i = 0; i < dev_cnt; ++i) {
        pjmedia_vid_dev_info info;
        status = pjmedia_vid_dev_get_info(i, &info);
        if (status == PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "%3d: %s - %s", i, info.driver, info.name));
        }
    }

    return PJ_SUCCESS;
}

static pj_status_t avi_play_cb(void *user_data, pjmedia_frame *frame)
{
    return pjmedia_port_get_frame((pjmedia_port*)user_data, frame);
}

static pj_status_t avi_event_cb(pjmedia_vid_stream *stream,
				void *user_data,
				pjmedia_vid_event *event)
{
    avi_port_t *ap = (avi_port_t *)user_data;

    PJ_UNUSED_ARG(stream);

    if (event->event_type != PJMEDIA_EVENT_MOUSEBUTTONDOWN)
        return PJ_SUCCESS;

    if (ap->is_running) {
        pjmedia_vid_port_stop(ap->vid_port);
        if (ap->aud_stream)
            pjmedia_aud_stream_stop(ap->aud_stream);
    } else {
        pjmedia_vid_port_start(ap->vid_port);
        if (ap->aud_stream)
            pjmedia_aud_stream_start(ap->aud_stream);
    }
    ap->is_running = !ap->is_running;

    /* We handled the event on our own, so return non-PJ_SUCCESS here */
    return -1;
}

static pj_status_t codec_get_frame(pjmedia_port *port,
			           pjmedia_frame *frame)
{
    codec_port_data_t *port_data = (codec_port_data_t*)port->port_data.pdata;
    pjmedia_vid_codec *codec = port_data->codec;
    pjmedia_frame enc_frame;
    pj_status_t status;

    enc_frame.buf = port_data->enc_buf;
    enc_frame.size = port_data->enc_buf_size;

    if (port_data->conv) {
        pj_size_t frame_size = frame->size;

        status = pjmedia_port_get_frame(port_data->src_port, frame);
        if (status != PJ_SUCCESS) goto on_error;

        status = codec->op->decode(codec, frame, frame->size, &enc_frame);
        if (status != PJ_SUCCESS) goto on_error;

        frame->size = frame_size;
        status = pjmedia_converter_convert(port_data->conv, &enc_frame, frame);
        if (status != PJ_SUCCESS) goto on_error;

        return PJ_SUCCESS;
    }

    status = pjmedia_port_get_frame(port_data->src_port, &enc_frame);
    if (status != PJ_SUCCESS) goto on_error;

    status = codec->op->decode(codec, &enc_frame, frame->size, frame);
    if (status != PJ_SUCCESS) goto on_error;

    return PJ_SUCCESS;

on_error:
    pj_perror(3, THIS_FILE, status, "codec_get_frame() error");
    return status;
}

static int aviplay_test(pj_pool_t *pool)
{
    pjmedia_vid_port *renderer=NULL;
    pjmedia_vid_port_param param;
    pjmedia_aud_param aparam;
    pjmedia_video_format_detail *vfd;
    pjmedia_audio_format_detail *afd;
    pjmedia_aud_stream *strm = NULL;
    pj_status_t status;
    int rc = 0;
    pjmedia_avi_streams *avi_streams;
    pjmedia_avi_stream *vid_stream, *aud_stream;
    pjmedia_port *vid_port = NULL, *aud_port = NULL;
    pjmedia_vid_codec *codec=NULL;
    avi_port_t avi_port;
#if PJ_WIN32
    const char *fname = "C:\\Users\\Liong Sauw Ming\\Desktop\\piratesmjpg.avi";
#else
    const char *fname = "/home/bennylp/Desktop/installer/video/movies/pirates.avi";
#endif

    pj_bzero(&avi_port, sizeof(avi_port));

    status = pjmedia_avi_player_create_streams(pool, fname, 0, &avi_streams);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(2,("", status, "    Error playing %s (ignored)", fname));
	rc = 210; goto on_return;
    }

    vid_stream = pjmedia_avi_streams_get_stream_by_media(avi_streams,
                                                         0,
                                                         PJMEDIA_TYPE_VIDEO);
    vid_port = pjmedia_avi_stream_get_port(vid_stream);

    if (vid_port) {
        pjmedia_vid_port_param_default(&param);

        status = pjmedia_vid_dev_default_param(pool,
                                               PJMEDIA_VID_DEFAULT_RENDER_DEV,
                                               &param.vidparam);
        if (status != PJ_SUCCESS) {
    	    rc = 220; goto on_return;
        }

        /* Create renderer, set it to active  */
        param.active = PJ_TRUE;
        param.vidparam.dir = PJMEDIA_DIR_RENDER;
        vfd = pjmedia_format_get_video_format_detail(&vid_port->info.fmt,
                                                     PJ_TRUE);
        pjmedia_format_init_video(&param.vidparam.fmt, 
                                  vid_port->info.fmt.id,
                                  vfd->size.w, vfd->size.h,
                                  vfd->fps.num, vfd->fps.denum);

        if (vid_port->info.fmt.id == PJMEDIA_FORMAT_MJPEG ||
            vid_port->info.fmt.id == PJMEDIA_FORMAT_H263)
        {
            /* Prepare codec */
            pj_str_t codec_id_st;
            unsigned info_cnt = 1, i, k;
            const pjmedia_vid_codec_info *codec_info;
            pj_str_t port_name = {"codec", 5};
            pj_uint8_t *enc_buf = NULL;
            pj_size_t enc_buf_size = 0;
            pjmedia_vid_dev_info rdr_info;
            pjmedia_port codec_port;
            codec_port_data_t codec_port_data;
            pjmedia_vid_codec_param codec_param;
            struct {
                pj_uint32_t     pjmedia_id;
                const char     *codec_id;
            } codec_fmts[] = 
                {{PJMEDIA_FORMAT_MJPEG, "mjpeg"},
                 {PJMEDIA_FORMAT_H263, "h263"}};
            const char *codec_id = NULL;


            status = pjmedia_codec_ffmpeg_init(NULL, mem);
            if (status != PJ_SUCCESS)
                return -20;

            /* Lookup codec */
            for (i = 0; i < sizeof(codec_fmts)/sizeof(codec_fmts[0]); i++) {
                if (vid_port->info.fmt.id == codec_fmts[i].pjmedia_id) {
                    codec_id = codec_fmts[i].codec_id;
                    break;
                }
            }
            if (!codec_id) {
                rc = 242; goto on_return;
            }
            pj_cstr(&codec_id_st, codec_id);
            status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL,
                                                             &codec_id_st, 
                                                             &info_cnt, 
                                                             &codec_info,
                                                             NULL);
            if (status != PJ_SUCCESS) {
                rc = 245; goto on_return;
            }
            status = pjmedia_vid_codec_mgr_get_default_param(NULL, codec_info,
                                                             &codec_param);
            if (status != PJ_SUCCESS) {
                rc = 246; goto on_return;
            }

            pjmedia_vid_dev_get_info(param.vidparam.rend_id, &rdr_info);
            for (i=0; i<codec_info->dec_fmt_id_cnt; ++i) {
                for (k=0; k<rdr_info.fmt_cnt; ++k) {
                    if (codec_info->dec_fmt_id[i]==(int)rdr_info.fmt[k].id)
                    {
                        param.vidparam.fmt.id = codec_info->dec_fmt_id[i];
                    }
                }
            }

            /* Open codec */
            status = pjmedia_vid_codec_mgr_alloc_codec(NULL, codec_info,
                                                       &codec);
            if (status != PJ_SUCCESS) {
                rc = 250; goto on_return;
            }

            status = codec->op->init(codec, pool);
            if (status != PJ_SUCCESS) {
                rc = 251; goto on_return;
            }

            pjmedia_format_copy(&codec_param.dec_fmt, &param.vidparam.fmt);

            status = codec->op->open(codec, &codec_param);
            if (status != PJ_SUCCESS) {
                rc = 252; goto on_return;
            }

            /* Alloc encoding buffer */
            enc_buf_size =  codec_param.dec_fmt.det.vid.size.w *
                            codec_param.dec_fmt.det.vid.size.h * 4
                            + 16; /*< padding, just in case */
            enc_buf = pj_pool_alloc(pool,enc_buf_size);

            /* Init codec port */
            pj_bzero(&codec_port, sizeof(codec_port));
            status = pjmedia_port_info_init2(&codec_port.info, &port_name,
                                             0x1234,
                                             PJMEDIA_DIR_ENCODING, 
                                             &codec_param.dec_fmt);
            if (status != PJ_SUCCESS) {
                rc = 260; goto on_return;
            }
            pj_bzero(&codec_port_data, sizeof(codec_port_data));
            codec_port_data.codec = codec;
            codec_port_data.src_port = vid_port;
            codec_port_data.enc_buf = enc_buf;
            codec_port_data.enc_buf_size = enc_buf_size;

            codec_port.get_frame = &codec_get_frame;
            codec_port.port_data.pdata = &codec_port_data;

            if (vid_port->info.fmt.id == PJMEDIA_FORMAT_MJPEG) {
                pjmedia_conversion_param conv_param;

                status = pjmedia_libswscale_converter_init(NULL, pool);

                pjmedia_format_copy(&conv_param.src, &param.vidparam.fmt);
                pjmedia_format_copy(&conv_param.dst, &param.vidparam.fmt);
                conv_param.dst.id = PJMEDIA_FORMAT_I420;
                param.vidparam.fmt.id = conv_param.dst.id;

                status = pjmedia_converter_create(NULL, pool, &conv_param,
                                                  &codec_port_data.conv);
                if (status != PJ_SUCCESS) {
                    rc = 270; goto on_return;
                }
            }

            status = pjmedia_vid_port_create(pool, &param, &renderer);
            if (status != PJ_SUCCESS) {
                rc = 230; goto on_return;
            }

            status = pjmedia_vid_port_connect(renderer, &codec_port,
                                              PJ_FALSE);
        } else {
            status = pjmedia_vid_port_create(pool, &param, &renderer);
            if (status != PJ_SUCCESS) {
                rc = 230; goto on_return;
            }

            /* Connect avi port to renderer */
            status = pjmedia_vid_port_connect(renderer, vid_port,
                                              PJ_FALSE);
        }

        if (status != PJ_SUCCESS) {
            rc = 240; goto on_return;
        }
    }

    aud_stream = pjmedia_avi_streams_get_stream_by_media(avi_streams,
                                                         0,
                                                         PJMEDIA_TYPE_AUDIO);
    aud_port = pjmedia_avi_stream_get_port(aud_stream);

    if (aud_port) {
        status = pjmedia_aud_dev_default_param(
                     PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV,
                     &aparam);
        if (status != PJ_SUCCESS) {
            rc = 310; goto on_return;
        }

        aparam.dir = PJMEDIA_DIR_PLAYBACK;
        afd = pjmedia_format_get_audio_format_detail(&aud_port->info.fmt,
                                                     PJ_TRUE);
        aparam.clock_rate = afd->clock_rate;
        aparam.channel_count = afd->channel_count;
        aparam.bits_per_sample = afd->bits_per_sample;
        aparam.samples_per_frame = afd->frame_time_usec * aparam.clock_rate *
                                   aparam.channel_count / 1000000;

        status = pjmedia_aud_stream_create(&aparam, NULL, &avi_play_cb,
                                           aud_port,
                                           &strm);
        if (status != PJ_SUCCESS) {
            rc = 320; goto on_return;
        }

        /* Start audio streaming.. */
        status = pjmedia_aud_stream_start(strm);
        if (status != PJ_SUCCESS) {
            rc = 330; goto on_return;
        }
    }

    if (vid_port) {
        pjmedia_vid_cb cb;

        pj_bzero(&cb, sizeof(cb));
        cb.on_event_cb = avi_event_cb;
        avi_port.aud_stream = strm;
        avi_port.vid_port = renderer;
        avi_port.is_running = PJ_TRUE;
        pjmedia_vid_port_set_cb(renderer, &cb, &avi_port);

        /* Start video streaming.. */
        status = pjmedia_vid_port_start(renderer);
        if (status != PJ_SUCCESS) {
            rc = 270; goto on_return;
        }
    }

#if VID_DEV_TEST_MAC_OS
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
    
    pj_thread_sleep(150000);

on_return:
    if (strm) {
	pjmedia_aud_stream_stop(strm);
	pjmedia_aud_stream_destroy(strm);
    }
    if (renderer)
        pjmedia_vid_port_destroy(renderer);
    if (vid_port)
        pjmedia_port_destroy(vid_port);
    if (codec) {
        codec->op->close(codec);
        pjmedia_vid_codec_mgr_dealloc_codec(NULL, codec);
    }

    return rc;
}

static int loopback_test(pj_pool_t *pool)
{
    pjmedia_vid_port *capture=NULL, *renderer=NULL;
    pjmedia_vid_port_param param;
    pjmedia_video_format_detail *vfd;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "  loopback test"));

    pjmedia_vid_port_param_default(&param);

    /* Create capture, set it to active (master) */
    status = pjmedia_vid_dev_default_param(pool,
                                           PJMEDIA_VID_DEFAULT_CAPTURE_DEV,
//                                           3, /* Hard-coded capture device */
					   &param.vidparam);
    if (status != PJ_SUCCESS) {
	rc = 100; goto on_return;
    }
    param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
    param.active = PJ_TRUE;

    if (param.vidparam.fmt.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO) {
	rc = 103; goto on_return;
    }

    vfd = pjmedia_format_get_video_format_detail(&param.vidparam.fmt, PJ_TRUE);
    if (vfd == NULL) {
	rc = 105; goto on_return;
    }

    status = pjmedia_vid_port_create(pool, &param, &capture);
    if (status != PJ_SUCCESS) {
	rc = 110; goto on_return;
    }

    /* Create renderer, set it to passive (slave)  */
    param.active = PJ_FALSE;
    param.vidparam.dir = PJMEDIA_DIR_RENDER;
    param.vidparam.rend_id = PJMEDIA_VID_DEFAULT_RENDER_DEV;
//    param.vidparam.rend_id = 6; /* Hard-coded render device */
    param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(pool, &param, &renderer);
    if (status != PJ_SUCCESS) {
	rc = 130; goto on_return;
    }

    /* Connect capture to renderer */
    status = pjmedia_vid_port_connect(capture,
				      pjmedia_vid_port_get_passive_port(renderer),
				      PJ_FALSE);
    if (status != PJ_SUCCESS) {
	rc = 140; goto on_return;
    }

    /* Start streaming.. */
    status = pjmedia_vid_port_start(renderer);
    if (status != PJ_SUCCESS) {
	rc = 150; goto on_return;
    }
    status = pjmedia_vid_port_start(capture);
    if (status != PJ_SUCCESS) {
	rc = 160; goto on_return;
    }

#if VID_DEV_TEST_MAC_OS
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
    
    /* Sleep while the webcam is being displayed... */
    pj_thread_sleep(20000);

on_return:
    PJ_PERROR(3, (THIS_FILE, status, "  error"));
    if (capture)
	pjmedia_vid_port_destroy(capture);
    if (renderer)
	pjmedia_vid_port_destroy(renderer);

    return rc;
}

int vid_dev_test(void)
{
    pj_pool_t *pool;
    int rc = 0;
    pj_status_t status;

#if VID_DEV_TEST_MAC_OS
    NSAutoreleasePool *apool = [[NSAutoreleasePool alloc] init];
    
    [NSApplication sharedApplication];
#endif
    
    PJ_LOG(3, (THIS_FILE, "Video device tests.."));

    pool = pj_pool_create(mem, "Viddev test", 256, 256, 0);

    status = pjmedia_vid_subsys_init(mem);
    if (status != PJ_SUCCESS)
        return -10;

    status = pjmedia_aud_subsys_init(mem);
    if (status != PJ_SUCCESS) {
        return -20;
    }

    rc = enum_devs();
    if (rc != 0)
	goto on_return;

    rc = aviplay_test(pool);
    //if (rc != 0)
    //    goto on_return;
    // Ignore error
    rc = 0;

    rc = loopback_test(pool);
    if (rc != 0)
	goto on_return;

on_return:
    pjmedia_aud_subsys_shutdown();
    pjmedia_vid_subsys_shutdown();
    pj_pool_release(pool);

#if VID_DEV_TEST_MAC_OS
    [apool release];
#endif
    
    return rc;
}



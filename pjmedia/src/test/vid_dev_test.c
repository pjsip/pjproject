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

static pj_bool_t is_quitting = PJ_FALSE;

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

static pj_status_t vid_event_cb(pjmedia_vid_dev_stream *stream,
				void *user_data,
				pjmedia_vid_event *event)
{
    PJ_UNUSED_ARG(stream);
    PJ_UNUSED_ARG(user_data);
    
    if (event->event_type == PJMEDIA_EVENT_WINDOW_CLOSE)
        is_quitting = PJ_TRUE;

    /* We will handle the event on our own, so return non-PJ_SUCCESS here */
    return -1;
}

static int loopback_test(pj_pool_t *pool)
{
    pjmedia_vid_port *capture=NULL, *renderer=NULL;
    pjmedia_vid_port_param param;
    pjmedia_video_format_detail *vfd;
    pjmedia_vid_cb cb;
    pj_status_t status;
    int rc = 0, i;

    PJ_LOG(3, (THIS_FILE, "  loopback test"));

    pjmedia_vid_port_param_default(&param);

    /* Create capture, set it to active (master) */
    status = pjmedia_vid_dev_default_param(pool,
                                           PJMEDIA_VID_DEFAULT_CAPTURE_DEV,
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
    param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(pool, &param, &renderer);
    if (status != PJ_SUCCESS) {
	rc = 130; goto on_return;
    }

    pj_bzero(&cb, sizeof(cb));
    cb.on_event_cb = vid_event_cb;
    pjmedia_vid_port_set_cb(renderer, &cb, NULL);

    /* Connect capture to renderer */
    status = pjmedia_vid_port_connect(
                 capture,
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

    /* Sleep while the webcam is being displayed... */
    for (i = 0; i < 15 && (!is_quitting); i++) {
#if VID_DEV_TEST_MAC_OS
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
        pj_thread_sleep(100);
    }

    /**
     * Test the renderer's format capability if the device
     * supports it.
     */
    if (pjmedia_vid_dev_stream_get_cap(pjmedia_vid_port_get_stream(renderer),
                                       PJMEDIA_VID_DEV_CAP_FORMAT,
                                       &param.vidparam.fmt) == PJ_SUCCESS)
    {
        status = pjmedia_vid_port_stop(capture);
        if (status != PJ_SUCCESS) {
            rc = 170; goto on_return;
        }
        status = pjmedia_vid_port_disconnect(capture);
        if (status != PJ_SUCCESS) {
            rc = 180; goto on_return;
        }
        pjmedia_vid_port_destroy(capture);

        param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
        param.active = PJ_TRUE;
        pjmedia_format_init_video(&param.vidparam.fmt, param.vidparam.fmt.id,
                                  640, 480,
                                  vfd->fps.num, vfd->fps.denum);
        vfd = pjmedia_format_get_video_format_detail(&param.vidparam.fmt,
                                                     PJ_TRUE);
        if (vfd == NULL) {
            rc = 185; goto on_return;
        }

        status = pjmedia_vid_port_create(pool, &param, &capture);
        if (status != PJ_SUCCESS) {
            rc = 190; goto on_return;
        }

        status = pjmedia_vid_port_connect(
                     capture,
                     pjmedia_vid_port_get_passive_port(renderer),
                     PJ_FALSE);
        if (status != PJ_SUCCESS) {
            rc = 200; goto on_return;
        }

        status = pjmedia_vid_dev_stream_set_cap(
                     pjmedia_vid_port_get_stream(renderer),
                     PJMEDIA_VID_DEV_CAP_FORMAT,
                     &param.vidparam.fmt);
        if (status != PJ_SUCCESS) {
            rc = 205; goto on_return;
        }

        status = pjmedia_vid_port_start(capture);
        if (status != PJ_SUCCESS) {
            rc = 210; goto on_return;
        }
    }

    for (i = 0; i < 35 && (!is_quitting); i++) {
#if VID_DEV_TEST_MAC_OS
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
#endif
        pj_thread_sleep(100);
    }

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

    rc = enum_devs();
    if (rc != 0)
	goto on_return;

    rc = loopback_test(pool);
    if (rc != 0)
	goto on_return;

on_return:
    pjmedia_vid_subsys_shutdown();
    pj_pool_release(pool);

#if VID_DEV_TEST_MAC_OS
    [apool release];
#endif
    
    return rc;
}

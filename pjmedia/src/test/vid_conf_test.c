/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/vid_conf.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#define THIS_FILE       "vid_conf_test.c"
#define SRC_CNT         6

static pj_status_t dummy_get_frame(pjmedia_port *port, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(port);
    frame->type = PJMEDIA_FRAME_TYPE_VIDEO;
    return PJ_SUCCESS;
}

static pj_status_t dummy_put_frame(pjmedia_port *port, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(port);
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}

static pj_status_t dummy_on_destroy(pjmedia_port *port)
{
    PJ_UNUSED_ARG(port);
    return PJ_SUCCESS;
}

static void init_port(pj_pool_t *pool, pjmedia_port *port, const char *name,
                      pjmedia_dir dir, unsigned w, unsigned h)
{
    pjmedia_format fmt;
    pj_str_t port_name;

    pj_bzero(port, sizeof(*port));
    pj_strdup2_with_null(pool, &port_name, name);
    pjmedia_format_init_video(&fmt, PJMEDIA_FORMAT_I420, w, h, 30, 1);
    pjmedia_port_info_init2(&port->info, &port_name,
                            PJMEDIA_SIG_CLASS_PORT('V', 'C', 'T'), dir, &fmt);
    if (dir & PJMEDIA_DIR_ENCODING)
        port->get_frame = &dummy_get_frame;
    if (dir & PJMEDIA_DIR_DECODING)
        port->put_frame = &dummy_put_frame;
    port->on_destroy = &dummy_on_destroy;
}

/* Wait until the bridge clock has applied the queued (dis)connections */
static int wait_transmitter_cnt(pjmedia_vid_conf *vid_conf, unsigned slot,
                                unsigned cnt)
{
    pjmedia_vid_conf_port_info info;
    unsigned retry;

    for (retry = 0; retry < 250; ++retry) {
        PJ_TEST_SUCCESS(pjmedia_vid_conf_get_port_info(vid_conf, slot, &info),
                        NULL, return -1);
        if (info.transmitter_cnt == cnt)
            break;
        pj_thread_sleep(20);
    }
    PJ_TEST_EQ(info.transmitter_cnt, cnt, NULL, return -2);

    /* Let the bridge render a few frames */
    pj_thread_sleep(100);
    return 0;
}

/* Connect more sources to one sink than the mixing layout can place,
 * then disconnect them one by one.
 */
static int connect_many_sources_test(void)
{
    pj_pool_t *pool;
    pjmedia_vid_conf *vid_conf = NULL;
    pjmedia_vid_conf_setting opt;
    pjmedia_port src_port[SRC_CNT], sink_port;
    unsigned src_slot[SRC_CNT], sink_slot;
    unsigned i, src_added = 0;
    pj_bool_t sink_added = PJ_FALSE;
    int rc = 0;

    pool = pj_pool_create(mem, "vidconftest", 1000, 1000, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -10);

    pjmedia_vid_conf_setting_default(&opt);
    PJ_TEST_SUCCESS(pjmedia_vid_conf_create(pool, &opt, &vid_conf),
                    NULL, {rc = -20; goto on_return;});

    init_port(pool, &sink_port, "sink", PJMEDIA_DIR_DECODING, 320, 240);
    PJ_TEST_SUCCESS(pjmedia_vid_conf_add_port(vid_conf, pool, &sink_port,
                                              NULL, NULL, &sink_slot),
                    NULL, {rc = -30; goto on_return;});
    sink_added = PJ_TRUE;

    for (i = 0; i < SRC_CNT; ++i) {
        char name[16];

        pj_ansi_snprintf(name, sizeof(name), "src%d", i);
        init_port(pool, &src_port[i], name, PJMEDIA_DIR_ENCODING,
                  160 + i * 16, 120 + i * 8);
        PJ_TEST_SUCCESS(pjmedia_vid_conf_add_port(vid_conf, pool,
                                                  &src_port[i], NULL, NULL,
                                                  &src_slot[i]),
                        NULL, {rc = -40; goto on_return;});
        ++src_added;
    }

    for (i = 0; i < SRC_CNT; ++i) {
        PJ_TEST_SUCCESS(pjmedia_vid_conf_connect_port(vid_conf, src_slot[i],
                                                      sink_slot, NULL),
                        NULL, {rc = -50; goto on_return;});
    }
    PJ_TEST_EQ(wait_transmitter_cnt(vid_conf, sink_slot, SRC_CNT), 0, NULL,
               {rc = -60; goto on_return;});

    for (i = 0; i < SRC_CNT; ++i) {
        PJ_TEST_SUCCESS(pjmedia_vid_conf_disconnect_port(vid_conf,
                                                         src_slot[i],
                                                         sink_slot),
                        NULL, {rc = -70; goto on_return;});
        PJ_TEST_EQ(wait_transmitter_cnt(vid_conf, sink_slot,
                                        SRC_CNT - 1 - i), 0, NULL,
                   {rc = -80; goto on_return;});
    }

on_return:
    if (vid_conf)
        pjmedia_vid_conf_destroy(vid_conf);
    if (sink_added)
        pjmedia_port_destroy(&sink_port);
    for (i = 0; i < src_added; ++i)
        pjmedia_port_destroy(&src_port[i]);
    pj_pool_release(pool);
    return rc;
}

int vid_conf_test(void)
{
    return connect_many_sources_test();
}

#else
int vid_conf_test(void)
{
    return 0;
}
#endif /* PJMEDIA_HAS_VIDEO */

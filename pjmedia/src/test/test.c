/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

#define THIS_FILE   "test.c"

pj_pool_factory *mem;
struct test_app_t test_app;


void app_perror(pj_status_t status, const char *msg)
{
    char errbuf[PJ_ERR_MSG_SIZE];
    
    pjmedia_strerror(status, errbuf, sizeof(errbuf));

    PJ_LOG(3,(THIS_FILE, "%s: %s", msg, errbuf));
}

/* Force linking PLC stuff if G.711 is disabled. See:
 *  https://github.com/pjsip/pjproject/issues/1337 
 */
#if PJMEDIA_HAS_G711_CODEC==0
void *dummy()
{
    // Dummy
    return &pjmedia_plc_save;
}
#endif

int test_main(int argc, char *argv[])
{
    int rc = 0;
    pj_caching_pool caching_pool;
    pj_pool_t *pool = NULL;

    PJ_TEST_SUCCESS(pj_init(), NULL, return 1);

    if (test_app.ut_app.prm_config)
        pj_dump_config();

    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    PJ_TEST_NOT_NULL(pool=pj_pool_create(&caching_pool.factory, "test",
                                         1000, 512, NULL),
                     NULL, {rc=10; goto on_return;});

    pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
                     PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT);
    pj_log_set_level(3);

    mem = &caching_pool.factory;

    PJ_TEST_SUCCESS(pjmedia_event_mgr_create(pool, 0, NULL),
                    NULL, {rc=30; goto on_return;});

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    PJ_TEST_SUCCESS(pjmedia_video_format_mgr_create(pool, 64, 0, NULL),
                    NULL, {rc=50; goto on_return;});
    PJ_TEST_SUCCESS(pjmedia_converter_mgr_create(pool, NULL),
                    NULL, {rc=60; goto on_return;});
    PJ_TEST_SUCCESS(pjmedia_vid_codec_mgr_create(pool, NULL),
                    NULL, {rc=70; goto on_return;});
#endif

    PJ_TEST_SUCCESS(ut_app_init1(&test_app.ut_app, mem), 
                    NULL, {rc=40; goto on_return;});

#if HAS_MIPS_TEST
    /* Run in exclusive mode to get the best performance */
    UT_ADD_TEST(&test_app.ut_app, mips_test, PJ_TEST_EXCLUSIVE);
#endif

#if HAS_VID_CODEC_TEST
    /* Run in exclusive mode due to device sharing error? */
    UT_ADD_TEST(&test_app.ut_app, vid_codec_test, PJ_TEST_EXCLUSIVE);
#endif

#if HAS_VID_PORT_TEST
    UT_ADD_TEST(&test_app.ut_app, vid_port_test, 0);
#endif

#if HAS_VID_DEV_TEST
    UT_ADD_TEST(&test_app.ut_app, vid_dev_test, 0);
#endif

#if HAS_SDP_NEG_TEST
    UT_ADD_TEST(&test_app.ut_app, sdp_neg_test, 0);
#endif
    //DO_TEST(sdp_test (&caching_pool.factory));
    //DO_TEST(rtp_test(&caching_pool.factory));
    //DO_TEST(session_test (&caching_pool.factory));
#if HAS_JBUF_TEST
    UT_ADD_TEST(&test_app.ut_app, jbuf_test, 0);
#endif
#if HAS_CODEC_VECTOR_TEST
    UT_ADD_TEST(&test_app.ut_app, codec_test_vectors, 0);
#endif

    if (ut_run_tests(&test_app.ut_app, "pjmedia tests", argc, argv)) {
        rc = 99;
    } else {
        rc = 0;
    }

    ut_app_destroy(&test_app.ut_app);

on_return:
    if (rc != 0) {
        PJ_LOG(3,(THIS_FILE,"Test completed with error(s)!"));
    } else {
        PJ_LOG(3,(THIS_FILE,"Looks like everything is okay!"));
    }

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    pjmedia_video_format_mgr_destroy(pjmedia_video_format_mgr_instance());
    pjmedia_converter_mgr_destroy(pjmedia_converter_mgr_instance());
    pjmedia_vid_codec_mgr_destroy(pjmedia_vid_codec_mgr_instance());
#endif

    pjmedia_event_mgr_destroy(pjmedia_event_mgr_instance());
    if (pool)
        pj_pool_release(pool);
    pj_caching_pool_destroy(&caching_pool);

    return rc;
}

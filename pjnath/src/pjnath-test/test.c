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
#include <pjlib.h>
#include <pj/compat/socket.h>

#define THIS_FILE   "test.c"

void app_perror_dbg(const char *msg, pj_status_t rc,
                    const char *file, int line)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(1,("test", "%s:%d: %s: [pj_status_t=%d] %s", file, line, msg,
              rc, errbuf));
}

/* Set socket to nonblocking. */
void app_set_sock_nb(pj_sock_t sock)
{
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    u_long value = 1;
    ioctlsocket(sock, FIONBIO, &value);
#else
    pj_uint32_t value = 1;
    ioctl(sock, FIONBIO, &value);
#endif
}

pj_status_t create_stun_config(app_sess_t *app_sess)
{
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer_heap = NULL;
    pj_lock_t *lock = NULL;
    pj_status_t status;

    pj_bzero(app_sess, sizeof(*app_sess));
    pj_caching_pool_init(&app_sess->cp, 
                         &pj_pool_factory_default_policy, 0 );
    app_sess->pool = pj_pool_create(&app_sess->cp.factory, NULL,
                                    512, 512, NULL);
    PJ_TEST_NOT_NULL(app_sess->pool, NULL,
                     { status=PJ_ENOMEM; goto on_error;});

    PJ_TEST_SUCCESS(pj_ioqueue_create(app_sess->pool, 64, &ioqueue), NULL, 
                    {status = tmp_status_; goto on_error;});
    PJ_TEST_SUCCESS(pj_timer_heap_create(app_sess->pool, 256, &timer_heap),
                    NULL, {status = tmp_status_; goto on_error;});

    pj_lock_create_recursive_mutex(app_sess->pool, NULL, &lock);
    pj_timer_heap_set_lock(timer_heap, lock, PJ_TRUE);
    lock = NULL;

    pj_stun_config_init(&app_sess->stun_cfg, app_sess->pool->factory, 0,
                        ioqueue, timer_heap);

    return PJ_SUCCESS;

on_error:
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (timer_heap)
        pj_timer_heap_destroy(timer_heap);
    // Lock should have been destroyed by timer heap.
    /*
    if (lock)
        pj_lock_destroy(lock);
    */
    if (app_sess->pool)
        pj_pool_release(app_sess->pool);
    pj_caching_pool_destroy(&app_sess->cp);
    return status;
}

void destroy_stun_config(app_sess_t *app_sess)
{
    if (app_sess->stun_cfg.timer_heap) {
        pj_timer_heap_destroy(app_sess->stun_cfg.timer_heap);
        app_sess->stun_cfg.timer_heap = NULL;
    }
    if (app_sess->stun_cfg.ioqueue) {
        pj_ioqueue_destroy(app_sess->stun_cfg.ioqueue);
        app_sess->stun_cfg.ioqueue = NULL;
    }
    if (app_sess->pool) {
        pj_pool_release(app_sess->pool);
        app_sess->pool = NULL;
    }
    pj_caching_pool_destroy(&app_sess->cp);
}

void poll_events(pj_stun_config *stun_cfg, unsigned msec,
                 pj_bool_t first_event_only)
{
    pj_time_val stop_time;
    int count = 0;

    pj_gettimeofday(&stop_time);
    stop_time.msec += msec;
    pj_time_val_normalize(&stop_time);

    /* Process all events for the specified duration. */
    for (;;) {
        pj_time_val timeout = {0, 1}, now;
        int c;

        c = pj_timer_heap_poll( stun_cfg->timer_heap, NULL );
        if (c > 0)
            count += c;

        //timeout.sec = timeout.msec = 0;
        c = pj_ioqueue_poll( stun_cfg->ioqueue, &timeout);
        if (c > 0)
            count += c;

        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, stop_time))
            break;

        if (first_event_only && count >= 0)
            break;
    }
}

void capture_pjlib_state(pj_stun_config *cfg, struct pjlib_state *st)
{
    pj_caching_pool *cp;

    st->timer_cnt = (unsigned)pj_timer_heap_count(cfg->timer_heap);

    cp = (pj_caching_pool*)cfg->pf;
    st->pool_used_cnt = (unsigned)cp->used_count;
}

int check_pjlib_state(pj_stun_config *cfg,
                      const struct pjlib_state *initial_st)
{
    struct pjlib_state current_state;
    int rc = 0;

    capture_pjlib_state(cfg, &current_state);

    if (current_state.timer_cnt > initial_st->timer_cnt) {
        rc |= ERR_TIMER_LEAK;

#if PJ_TIMER_DEBUG
        pj_timer_heap_dump(cfg->timer_heap);
#endif
        PJ_LOG(3,("", "    error: possibly leaking timer"));
    }

    if (current_state.pool_used_cnt > initial_st->pool_used_cnt) {
        PJ_LOG(3,("", "    dumping memory pool:"));
        pj_pool_factory_dump(cfg->pf, PJ_TRUE);
        PJ_LOG(3,("", "    error: possibly leaking memory"));
        rc |= ERR_MEMORY_LEAK;
    }

    return rc;
}


pj_pool_factory *mem;
struct test_app_t test_app;

int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | PJ_LOG_HAS_SENDER |
                      PJ_LOG_HAS_MICRO_SEC;

pj_log_func *orig_log_func;
FILE *log_file;

static void test_log_func(int level, const char *data, int len)
{
    if (log_file) {
        fwrite(data, len, 1, log_file);
    }
    if (level <= 3)
        orig_log_func(level, data, len);
}

static int test_inner(int argc, char *argv[])
{
    pj_caching_pool caching_pool;
    int i, rc = 0;

    mem = &caching_pool.factory;

#if 1
    pj_log_set_level(3);
    pj_log_set_decor(param_log_decor);
    PJ_UNUSED_ARG(test_log_func);
#elif 1
    log_file = fopen("pjnath-test.log", "wt");
    pj_log_set_level(5);
    orig_log_func = pj_log_get_log_func();
    pj_log_set_log_func(&test_log_func);
#endif

    PJ_TEST_SUCCESS(pj_init(), NULL, 
                    {if (log_file) fclose(log_file); return 1; });

    if (test_app.ut_app.prm_config)
        pj_dump_config();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

    PJ_TEST_SUCCESS(pjlib_util_init(), NULL, {rc=2; goto on_return;});
    PJ_TEST_SUCCESS(pjnath_init(), NULL, {rc=3; goto on_return;});
    PJ_TEST_SUCCESS(ut_app_init1(&test_app.ut_app, mem), 
                    NULL, {rc=4; goto on_return;});

#if INCLUDE_STUN_TEST
    UT_ADD_TEST(&test_app.ut_app, stun_test, 0);
    UT_ADD_TEST(&test_app.ut_app, sess_auth_test, 0);
#endif

#if INCLUDE_STUN_SOCK_TEST
    UT_ADD_TEST(&test_app.ut_app, stun_sock_test, 0);
#endif

#if INCLUDE_ICE_TEST
    for (i=0; i<ICE_TEST_START_ARRAY+ICE_TEST_ARRAY_COUNT; ++i)
        UT_ADD_TEST1(&test_app.ut_app, ice_test, (void*)(long)i, 0);
#endif

#if INCLUDE_TRICKLE_ICE_TEST
    UT_ADD_TEST(&test_app.ut_app, trickle_ice_test, 0);
#endif

#if INCLUDE_TURN_SOCK_TEST
    UT_ADD_TEST1(&test_app.ut_app, turn_sock_test, (void*)(long)0, 0);
    UT_ADD_TEST1(&test_app.ut_app, turn_sock_test, (void*)(long)1, 0);
    UT_ADD_TEST1(&test_app.ut_app, turn_sock_test, (void*)(long)2, 0);
#endif

#if INCLUDE_CONCUR_TEST
    UT_ADD_TEST(&test_app.ut_app, ice_conc_test, 0);

    for (i=0; i<50; ++i) {
        UT_ADD_TEST(&test_app.ut_app, concur_test, 0);
    }
#else
    PJ_UNUSED_ARG(i);
#endif

    if (ut_run_tests(&test_app.ut_app, "pjnath tests", argc, argv)) {
        rc = 5;
    } else {
        rc = 0;
    }

    ut_app_destroy(&test_app.ut_app);

on_return:
    if (log_file)
        fclose(log_file);
    pj_caching_pool_destroy( &caching_pool );
    return rc;
}

int test_main(int argc, char *argv[])
{
    PJ_USE_EXCEPTION;

    PJ_TRY {
        return test_inner(argc, argv);
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,("test", "FATAL: unhandled exception id %d (%s)",
                  id, pj_exception_id_name(id)));
    }
    PJ_END;

    return -1;
}


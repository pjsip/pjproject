/* $Id$ */
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

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(1,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
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

pj_status_t create_stun_config(pj_pool_t *pool, pj_stun_config *stun_cfg)
{
    pj_ioqueue_t *ioqueue;
    pj_timer_heap_t *timer_heap;
    pj_lock_t *lock;
    pj_status_t status;

    status = pj_ioqueue_create(pool, 64, &ioqueue);
    if (status != PJ_SUCCESS) {
	app_perror("   pj_ioqueue_create()", status);
	return status;
    }

    status = pj_timer_heap_create(pool, 256, &timer_heap);
    if (status != PJ_SUCCESS) {
	app_perror("   pj_timer_heap_create()", status);
	pj_ioqueue_destroy(ioqueue);
	return status;
    }

    pj_lock_create_recursive_mutex(pool, NULL, &lock);
    pj_timer_heap_set_lock(timer_heap, lock, PJ_TRUE);

    pj_stun_config_init(stun_cfg, mem, 0, ioqueue, timer_heap);

    return PJ_SUCCESS;
}

void destroy_stun_config(pj_stun_config *stun_cfg)
{
    if (stun_cfg->timer_heap) {
	pj_timer_heap_destroy(stun_cfg->timer_heap);
	stun_cfg->timer_heap = NULL;
    }
    if (stun_cfg->ioqueue) {
	pj_ioqueue_destroy(stun_cfg->ioqueue);
	stun_cfg->ioqueue = NULL;
    }
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
	PJ_LOG(3,("", "    error: possibly leaking timer"));
	rc |= ERR_TIMER_LEAK;

#if PJ_TIMER_DEBUG
	pj_timer_heap_dump(cfg->timer_heap);
#endif
    }

    if (current_state.pool_used_cnt > initial_st->pool_used_cnt) {
	PJ_LOG(3,("", "    error: possibly leaking memory"));
	PJ_LOG(3,("", "    dumping memory pool:"));
	pj_pool_factory_dump(mem, PJ_TRUE);
	rc |= ERR_MEMORY_LEAK;
    }

    return rc;
}


#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (char*)(rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)


pj_pool_factory *mem;

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

static int test_inner(void)
{
    pj_caching_pool caching_pool;
    int rc = 0;

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

    rc = pj_init();
    if (rc != 0) {
	app_perror("pj_init() error!!", rc);
	return rc;
    }

    pj_dump_config();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

    pjlib_util_init();
    pjnath_init();

#if INCLUDE_STUN_TEST
    DO_TEST(stun_test());
    DO_TEST(sess_auth_test());
#endif

#if INCLUDE_ICE_TEST
    DO_TEST(ice_test());
#endif

#if INCLUDE_STUN_SOCK_TEST
    DO_TEST(stun_sock_test());
#endif

#if INCLUDE_TURN_SOCK_TEST
    DO_TEST(turn_sock_test());
#endif

#if INCLUDE_CONCUR_TEST
    DO_TEST(concur_test());
#endif

on_return:
    if (log_file)
	fclose(log_file);
    return rc;
}

int test_main(void)
{
    PJ_USE_EXCEPTION;

    PJ_TRY {
        return test_inner();
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,("test", "FATAL: unhandled exception id %d (%s)",
                  id, pj_exception_id_name(id)));
    }
    PJ_END;

    return -1;
}


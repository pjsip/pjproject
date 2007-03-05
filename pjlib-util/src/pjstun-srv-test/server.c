/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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

#include "server.h"

#define THIS_FILE	"server.c"

struct pj_stun_server
{
    pj_stun_server_info	si;

    pj_pool_t		*pool;

    pj_bool_t		 thread_quit_flag;
    pj_thread_t	       **threads;
};

PJ_DEF(pj_status_t) pj_stun_perror( const char *sender, 
				    const char *title, 
				    pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(sender, "%s: %s", title, errmsg));
    return status;
}

static int worker_thread(void *p)
{
    pj_stun_server *srv = (pj_stun_server*)p;

    while (!srv->thread_quit_flag) {
	pj_time_val timeout = { 0, 50 };
	pj_timer_heap_poll(srv->si.timer_heap, NULL);
	pj_ioqueue_poll(srv->si.ioqueue, &timeout);
    }

    return 0;
}


PJ_DEF(pj_status_t) pj_stun_server_create(pj_pool_factory *pf,
					  unsigned thread_cnt,
					  pj_stun_server **p_srv)
{
    pj_pool_t *pool;
    pj_stun_server *srv;
    unsigned i;
    pj_status_t status;

    pool = pj_pool_create(pf, "server%p", 4000, 4000, NULL);

    srv = PJ_POOL_ZALLOC_T(pool, pj_stun_server);
    srv->pool = pool;
    srv->si.pf = pf;

    status = pj_ioqueue_create(srv->pool, PJ_IOQUEUE_MAX_HANDLES, 
			       &srv->si.ioqueue);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_timer_heap_create(srv->pool, 1024, &srv->si.timer_heap);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_stun_endpoint_create(srv->si.pf, 0, srv->si.ioqueue, 
				     srv->si.timer_heap, &srv->si.endpt);
    if (status != PJ_SUCCESS)
	goto on_error;

    srv->si.thread_cnt = thread_cnt;
    srv->threads = pj_pool_calloc(pool, thread_cnt, sizeof(pj_thread_t*));
    for (i=0; i<thread_cnt; ++i) {
	status = pj_thread_create(pool, "worker%p", &worker_thread,
				  srv, 0, 0, &srv->threads[i]);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    *p_srv = srv;
    return PJ_SUCCESS;

on_error:
    pj_stun_server_destroy(srv);
    return status;
}


PJ_DEF(pj_stun_server_info*) pj_stun_server_get_info(pj_stun_server *srv)
{
    return &srv->si;
}


PJ_DEF(pj_status_t) pj_stun_server_destroy(pj_stun_server *srv)
{
    unsigned i;

    srv->thread_quit_flag = PJ_TRUE;
    for (i=0; i<srv->si.thread_cnt; ++i) {
	pj_thread_join(srv->threads[i]);
	srv->threads[i] = NULL;
    }

    pj_stun_endpoint_destroy(srv->si.endpt);
    pj_timer_heap_destroy(srv->si.timer_heap);
    pj_ioqueue_destroy(srv->si.ioqueue);
    pj_pool_release(srv->pool);

    return PJ_SUCCESS;
}



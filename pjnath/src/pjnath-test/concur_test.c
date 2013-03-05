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

#if INCLUDE_CONCUR_TEST

#define THIS_FILE	"concur_test.c"

/****************************************************************************/
#define WORKER_THREAD_CNT	4
#define SERVER_THREAD_CNT	4
#define MAX_SOCK_CLIENTS	80

struct stun_test_session
{
    pj_stun_config	 stun_cfg;

    pj_lock_t		*lock;

    pj_thread_t		*worker_threads[WORKER_THREAD_CNT];

    pj_sock_t		 server_sock;
    int			 server_port;
    pj_thread_t		*server_threads[SERVER_THREAD_CNT];
    pj_event_t		*server_event;

    pj_bool_t		 thread_quit_flag;

    /* Test parameters:  */
    struct {
	int		 client_got_response;

	pj_bool_t	 server_wait_for_event;
	pj_bool_t	 server_drop_request;
	int		 client_sleep_after_start;
	int		 client_sleep_before_destroy;
    } param;
};

static int server_thread_proc(void *p)
{
    struct stun_test_session *test_sess = (struct stun_test_session*)p;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "Server thread running"));

    pool = pj_pool_create(test_sess->stun_cfg.pf, "server", 512, 512, NULL);

    while (!test_sess->thread_quit_flag) {
	pj_time_val timeout = {0, 10};
	pj_fd_set_t rdset;
	int n;

	/* Serve client */
	PJ_FD_ZERO(&rdset);
	PJ_FD_SET(test_sess->server_sock, &rdset);
	n = pj_sock_select(test_sess->server_sock+1, &rdset,
	                   NULL, NULL, &timeout);
	if (n==1 && PJ_FD_ISSET(test_sess->server_sock, &rdset)) {
	    pj_uint8_t pkt[512];
	    pj_ssize_t pkt_len;
	    pj_size_t res_len;
	    pj_sockaddr client_addr;
	    int addr_len;

	    pj_stun_msg	*stun_req, *stun_res;

	    pj_pool_reset(pool);

	    /* Got query */
	    pkt_len = sizeof(pkt);
	    addr_len = sizeof(client_addr);
	    status = pj_sock_recvfrom(test_sess->server_sock, pkt, &pkt_len,
	                              0, &client_addr, &addr_len);
	    if (status != PJ_SUCCESS) {
		continue;
	    }

	    status = pj_stun_msg_decode(pool, pkt, pkt_len,
	                                PJ_STUN_IS_DATAGRAM,
	                                &stun_req, NULL, NULL);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(1,(THIS_FILE, status, "STUN request decode error"));
		continue;
	    }

	    status = pj_stun_msg_create_response(pool, stun_req,
	                                         PJ_STUN_SC_BAD_REQUEST, NULL,
	                                         &stun_res);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(1,(THIS_FILE, status, "STUN create response error"));
		continue;
	    }

	    status = pj_stun_msg_encode(stun_res, pkt, sizeof(pkt), 0,
	                                NULL, &res_len);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(1,(THIS_FILE, status, "STUN encode error"));
		continue;
	    }

	    /* Ignore request */
	    if (test_sess->param.server_drop_request)
		continue;

	    /* Wait for signal to continue */
	    if (test_sess->param.server_wait_for_event)
		pj_event_wait(test_sess->server_event);

	    pkt_len = res_len;
	    pj_sock_sendto(test_sess->server_sock, pkt, &pkt_len, 0,
	                   &client_addr, pj_sockaddr_get_len(&client_addr));
	}
    }

    pj_pool_release(pool);

    PJ_LOG(4,(THIS_FILE, "Server thread quitting"));
    return 0;
}

static int worker_thread_proc(void *p)
{
    struct stun_test_session *test_sess = (struct stun_test_session*)p;

    PJ_LOG(4,(THIS_FILE, "Worker thread running"));

    while (!test_sess->thread_quit_flag) {
	pj_time_val timeout = {0, 10};
	pj_timer_heap_poll(test_sess->stun_cfg.timer_heap, NULL);
	pj_ioqueue_poll(test_sess->stun_cfg.ioqueue, &timeout);
    }

    PJ_LOG(4,(THIS_FILE, "Worker thread quitting"));
    return 0;
}

static pj_bool_t stun_sock_on_status(pj_stun_sock *stun_sock,
                                     pj_stun_sock_op op,
                                     pj_status_t status)
{
    struct stun_test_session *test_sess = (struct stun_test_session*)pj_stun_sock_get_user_data(stun_sock);

    PJ_UNUSED_ARG(op);
    PJ_UNUSED_ARG(status);

    test_sess->param.client_got_response++;
    return PJ_TRUE;
}

static int stun_destroy_test_session(struct stun_test_session *test_sess)
{

    unsigned i;
    pj_stun_sock_cb stun_cb;
    pj_status_t status;
    pj_stun_sock *stun_sock[MAX_SOCK_CLIENTS];

    pj_bzero(&stun_cb, sizeof(stun_cb));
    stun_cb.on_status = &stun_sock_on_status;

    pj_event_reset(test_sess->server_event);

    /* Create all clients first */
    for (i=0; i<MAX_SOCK_CLIENTS; ++i) {
	char name[10];
	sprintf(name, "stun%02d", i);
	status = pj_stun_sock_create(&test_sess->stun_cfg, name, pj_AF_INET(),
	                             &stun_cb, NULL, test_sess,
	                             &stun_sock[i]);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error creating stun socket"));
	    return -10;
	}
    }

    /* Start resolution */
    for (i=0; i<MAX_SOCK_CLIENTS; ++i) {
	pj_str_t server_ip = pj_str("127.0.0.1");
	status = pj_stun_sock_start(stun_sock[i], &server_ip,
	                            (pj_uint16_t)test_sess->server_port, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error starting stun socket"));
	    return -20;
	}
    }

    /* settle down */
    pj_thread_sleep(test_sess->param.client_sleep_after_start);

    /* Resume server threads */
    pj_event_set(test_sess->server_event);

    pj_thread_sleep(test_sess->param.client_sleep_before_destroy);

    /* Destroy clients */
    for (i=0; i<MAX_SOCK_CLIENTS; ++i) {
	status = pj_stun_sock_destroy(stun_sock[i]);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error destroying stun socket"));
	}
    }

    return 0;
}

static int stun_destroy_test(void)
{
    enum { LOOP = 500 };
    struct stun_test_session test_sess;
    pj_sockaddr bind_addr;
    int addr_len;
    pj_caching_pool cp;
    pj_pool_t *pool;
    unsigned i;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  STUN destroy concurrency test"));

    pj_bzero(&test_sess, sizeof(test_sess));

    pj_caching_pool_init(&cp, NULL, 0);
    pool = pj_pool_create(&cp.factory, "testsess", 512, 512, NULL);

    pj_stun_config_init(&test_sess.stun_cfg, &cp.factory, 0, NULL, NULL);

    status = pj_timer_heap_create(pool, 1023, &test_sess.stun_cfg.timer_heap);
    pj_assert(status == PJ_SUCCESS);

    status = pj_lock_create_recursive_mutex(pool, NULL, &test_sess.lock);
    pj_assert(status == PJ_SUCCESS);

    pj_timer_heap_set_lock(test_sess.stun_cfg.timer_heap, test_sess.lock, PJ_TRUE);
    pj_assert(status == PJ_SUCCESS);

    status = pj_ioqueue_create(pool, 512, &test_sess.stun_cfg.ioqueue);
    pj_assert(status == PJ_SUCCESS);

    pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &test_sess.server_sock);
    pj_sockaddr_init(pj_AF_INET(), &bind_addr, NULL, 0);
    status = pj_sock_bind(test_sess.server_sock, &bind_addr, pj_sockaddr_get_len(&bind_addr));
    pj_assert(status == PJ_SUCCESS);

    addr_len = sizeof(bind_addr);
    status = pj_sock_getsockname(test_sess.server_sock, &bind_addr, &addr_len);
    pj_assert(status == PJ_SUCCESS);

    test_sess.server_port = pj_sockaddr_get_port(&bind_addr);

    status = pj_event_create(pool, NULL, PJ_TRUE, PJ_FALSE, &test_sess.server_event);
    pj_assert(status == PJ_SUCCESS);

    for (i=0; i<SERVER_THREAD_CNT; ++i) {
	status = pj_thread_create(pool, NULL,
	                          &server_thread_proc, &test_sess,
	                          0, 0, &test_sess.server_threads[i]);
	pj_assert(status == PJ_SUCCESS);
    }

    for (i=0; i<WORKER_THREAD_CNT; ++i) {
	status = pj_thread_create(pool, NULL,
	                          &worker_thread_proc, &test_sess,
	                          0, 0, &test_sess.worker_threads[i]);
	pj_assert(status == PJ_SUCCESS);
    }

    /* Test 1: Main thread calls destroy while callback is processing response */
    PJ_LOG(3,(THIS_FILE, "    Destroy in main thread while callback is running"));
    for (i=0; i<LOOP; ++i) {
	int sleep = pj_rand() % 5;

	PJ_LOG(3,(THIS_FILE, "      Try %-3d of %d", i+1, LOOP));

	/* Test 1: destroy at the same time when receiving response */
	pj_bzero(&test_sess.param, sizeof(test_sess.param));
	test_sess.param.client_sleep_after_start = 20;
	test_sess.param.client_sleep_before_destroy = sleep;
	test_sess.param.server_wait_for_event = PJ_TRUE;
	stun_destroy_test_session(&test_sess);
	PJ_LOG(3,(THIS_FILE,
		  "        stun test a: sleep delay:%d: clients with response: %d",
		  sleep, test_sess.param.client_got_response));

	/* Test 2: destroy at the same time with STUN retransmit timer */
	test_sess.param.server_drop_request = PJ_TRUE;
	test_sess.param.client_sleep_after_start = 0;
	test_sess.param.client_sleep_before_destroy = PJ_STUN_RTO_VALUE;
	test_sess.param.server_wait_for_event = PJ_FALSE;
	stun_destroy_test_session(&test_sess);
	PJ_LOG(3,(THIS_FILE, "        stun test b: retransmit concurrency"));

	/* Test 3: destroy at the same time with receiving response
	 * AND STUN retransmit timer */
	test_sess.param.client_got_response = 0;
	test_sess.param.server_drop_request = PJ_FALSE;
	test_sess.param.client_sleep_after_start = PJ_STUN_RTO_VALUE;
	test_sess.param.client_sleep_before_destroy = 0;
	test_sess.param.server_wait_for_event = PJ_TRUE;
	stun_destroy_test_session(&test_sess);
	PJ_LOG(3,(THIS_FILE,
		  "        stun test c: clients with response: %d",
		  test_sess.param.client_got_response));

	pj_thread_sleep(10);

	ice_one_conc_test(&test_sess.stun_cfg, PJ_FALSE);

	pj_thread_sleep(10);
    }

    /* Avoid compiler warning */
    goto on_return;


on_return:
    test_sess.thread_quit_flag = PJ_TRUE;

    for (i=0; i<SERVER_THREAD_CNT; ++i) {
	pj_thread_join(test_sess.server_threads[i]);
    }

    for (i=0; i<WORKER_THREAD_CNT; ++i) {
	pj_thread_join(test_sess.worker_threads[i]);
    }

    pj_event_destroy(test_sess.server_event);
    pj_sock_close(test_sess.server_sock);
    pj_ioqueue_destroy(test_sess.stun_cfg.ioqueue);
    pj_timer_heap_destroy(test_sess.stun_cfg.timer_heap);

    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);

    PJ_LOG(3,(THIS_FILE, "    Done. rc=%d", rc));
    return rc;
}


int concur_test(void)
{
    int rc = 0;

    rc += stun_destroy_test();

    return 0;
}

#endif	/* INCLUDE_CONCUR_TEST */

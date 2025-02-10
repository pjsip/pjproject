/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2024 jimying at github dot com.
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
 
 
 /*
 * IOCP crash reproduce test (issue #985).
 * The code is taken from PR #4172 with few minor changes.
 * Issue fix attempt is done in #4136.
 *
 * Note:
 * - The crash was reproducible on Windows 10 & MSVC2005 (Win32),
 *   but not reproducible on Windows 11 & MSVC2022 (Win32 &x64).
 * - Test can be run for any ioqueue and normally take less than one second.
 */
#include <pjlib.h>
#include "test.h"

#define THIS_FILE "iocp_unregister_test.c"

#define CLIENT_NUM (PJ_IOQUEUE_MAX_HANDLES-1)

/**
 * socket info
 * has an independent memory pool, which is the key to successful
 * reproduce crash
 */
struct sock_info_t {
    pj_pool_t *pool;
    pj_activesock_t *asock;
    pj_sockaddr bound_addr;
};

struct iocp_test_t {
    pj_pool_t *pool;
    pj_ioqueue_t *ioq;
    pj_thread_t *tid;
    pj_bool_t quit;
    struct sock_info_t *socks[CLIENT_NUM];
    pj_activesock_t *asock_send;
};


static int worker_thread(void *p)
{
    struct iocp_test_t *test = (struct iocp_test_t *)p;

    while(!test->quit) {
        pj_time_val timeout = {0, 100};
        pj_ioqueue_poll(test->ioq, &timeout);
    }

    return 0;
}

static unsigned recv_cnt;

static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
                                  void *data,
                                  pj_size_t size,
                                  const pj_sockaddr_t *src_addr,
                                  int addr_len,
                                  pj_status_t status)
{
    (void)asock;
    (void)src_addr;
    (void)addr_len;
    PJ_LOG(3, (THIS_FILE, "on_data_recvfrom() data:%.*s, status:%d",
                          (int)size, (char *)data, status));
    ++recv_cnt;
    return PJ_TRUE;
}

int iocp_unregister_test(void)
{
    struct iocp_test_t *test;
    pj_pool_t *pool;
    pj_status_t status;
    unsigned i;
    pj_activesock_cfg cfg;
    pj_activesock_cb cb;
    struct sock_info_t *sock_info;
    pj_sockaddr loc_addr;
    pj_str_t loop = {"127.0.0.1", 9};

    // Let's just do it for any ioqueue.
    //if (strcmp(pj_ioqueue_name(), "iocp")) {
    //    /* skip if ioqueue framework is not iocp */
    //    return PJ_SUCCESS;
    //}

    pool = pj_pool_create(mem, "iocp-crash-test", 500, 500, NULL);
    test = PJ_POOL_ZALLOC_T(pool, struct iocp_test_t);
    test->pool = pool;
    status = pj_ioqueue_create(pool, CLIENT_NUM+1, &test->ioq);
    if (status != PJ_SUCCESS) {
        status = -900;
        goto on_error;
    }

    status = pj_thread_create(pool, "iocp-crash-test", worker_thread,
                              test, 0, 0, &test->tid);
    if (status != PJ_SUCCESS) {
        status = -901;
        goto on_error;
    }

    pj_activesock_cfg_default(&cfg);
    pj_bzero(&cb, sizeof(cb));
    cb.on_data_recvfrom = on_data_recvfrom;

    /* create send socket */
    status = pj_activesock_create_udp(pool, NULL, &cfg, test->ioq, &cb, NULL,
                                      &test->asock_send, NULL);
    if (status != PJ_SUCCESS) {
        status = -902;
        goto on_error;
    }

    /* create sockets to receive */
    pj_sockaddr_init(pj_AF_INET(), &loc_addr, &loop, 0);
    for (i = 0; i < PJ_ARRAY_SIZE(test->socks); i++) {
        pool = pj_pool_create(mem, "sock%p", 500, 500, NULL);
        sock_info = PJ_POOL_ZALLOC_T(pool, struct sock_info_t);
        sock_info->pool = pool;

        status = pj_activesock_create_udp(pool, &loc_addr, &cfg, test->ioq,
                                          &cb, NULL, &sock_info->asock,
                                          &sock_info->bound_addr);
        if (status != PJ_SUCCESS) {
            status = -903;
            pj_pool_release(pool);
            goto on_error;
        }
        test->socks[i] = sock_info;
        pj_activesock_start_recvfrom(sock_info->asock, pool, 256, 0);
    }

    /* send 'hello' to every socks */
    for (i = 0; i < PJ_ARRAY_SIZE(test->socks); i++) {
        pj_ioqueue_op_key_t *send_key;
        pj_str_t data;
        pj_ssize_t sent;

        sock_info = test->socks[i];
        send_key = PJ_POOL_ZALLOC_T(test->pool, pj_ioqueue_op_key_t);
        pj_strdup2_with_null(test->pool, &data, "hello");
        sent = data.slen;
        status = pj_activesock_sendto(test->asock_send, send_key, data.ptr,
                                &sent, 0, &sock_info->bound_addr,
                                pj_sockaddr_get_len(&sock_info->bound_addr));
        if (status != PJ_SUCCESS && status != PJ_EPENDING) {
            char buf[80];
            pj_sockaddr_print(&sock_info->bound_addr, buf, sizeof(buf), 3);
            PJ_PERROR(2, (THIS_FILE, status, "send error, dest:%s", buf));
        }
    }

    pj_thread_sleep(20);

    /* close all socks */
    for (i = 0; i < PJ_ARRAY_SIZE(test->socks); i++) {
        sock_info = test->socks[i];
        pj_activesock_close(sock_info->asock);
        pj_pool_release(sock_info->pool);
        test->socks[i] = NULL;
    }

    pj_thread_sleep(20);

    /* quit */
    test->quit = PJ_TRUE;
    status = PJ_SUCCESS;

on_error:
    if (test->tid)
        pj_thread_join(test->tid);
    for (i = 0; i < PJ_ARRAY_SIZE(test->socks); i++) {
        sock_info = test->socks[i];
        if (!sock_info)
            break;
        pj_activesock_close(sock_info->asock);
        pj_pool_release(sock_info->pool);
    }
    if(test->asock_send)
        pj_activesock_close(test->asock_send);
    if (test->ioq)
        pj_ioqueue_destroy(test->ioq);
    pj_pool_release(test->pool);

    PJ_LOG(3, (THIS_FILE, "Recv cnt = %u", recv_cnt));
    return status;
}

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

/**
 * \page page_pjlib_activesock_test Test: Active Socket
 *
 * This file is <b>pjlib-test/activesock.c</b>
 *
 * \include pjlib-test/activesock.c
 */

#if INCLUDE_ACTIVESOCK_TEST

#define THIS_FILE   "activesock.c"

#define ERR(r__)  { ret=r__; goto on_return; }

/*******************************************************************
 * Simple UDP echo server.
 */
struct udp_echo_srv
{
    pj_activesock_t     *asock;
    pj_bool_t            echo_enabled;
    pj_uint16_t          port;
    pj_ioqueue_op_key_t  send_key;
    pj_status_t          status;
    unsigned             rx_cnt;
    unsigned             rx_err_cnt, tx_err_cnt;
};

static void udp_echo_err(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,(THIS_FILE, "   error: %s: %s", title, errmsg));
}

static pj_bool_t udp_echo_srv_on_data_recvfrom(pj_activesock_t *asock,
                                               void *data,
                                               pj_size_t size,
                                               const pj_sockaddr_t *src_addr,
                                               int addr_len,
                                               pj_status_t status)
{
    struct udp_echo_srv *srv;
    pj_ssize_t sent;


    srv = (struct udp_echo_srv*) pj_activesock_get_user_data(asock);

    if (status != PJ_SUCCESS) {
        srv->status = status;
        srv->rx_err_cnt++;
        udp_echo_err("recvfrom() callback", status);
        return PJ_TRUE;
    }

    srv->rx_cnt++;

    /* Send back if echo is enabled */
    if (srv->echo_enabled) {
        sent = size;
        srv->status = pj_activesock_sendto(asock, &srv->send_key, data, 
                                           &sent, 0,
                                           src_addr, addr_len);
        if (srv->status != PJ_SUCCESS) {
            srv->tx_err_cnt++;
            udp_echo_err("sendto()", status);
        }
    }

    return PJ_TRUE;
}


static pj_status_t udp_echo_srv_create(pj_pool_t *pool,
                                       pj_ioqueue_t *ioqueue,
                                       pj_bool_t enable_echo,
                                       struct udp_echo_srv **p_srv)
{
    struct udp_echo_srv *srv;
    pj_sockaddr addr;
    pj_activesock_cb activesock_cb;
    pj_status_t status;

    srv = PJ_POOL_ZALLOC_T(pool, struct udp_echo_srv);
    srv->echo_enabled = enable_echo;

    pj_sockaddr_in_init(&addr.ipv4, NULL, 0);

    pj_bzero(&activesock_cb, sizeof(activesock_cb));
    activesock_cb.on_data_recvfrom = &udp_echo_srv_on_data_recvfrom;

    status = pj_activesock_create_udp(pool, &addr, NULL, ioqueue, &activesock_cb, 
                                      srv, &srv->asock, &addr);
    if (status != PJ_SUCCESS) {
        udp_echo_err("pj_activesock_create()", status);
        return status;
    }

    srv->port = pj_ntohs(addr.ipv4.sin_port);

    pj_ioqueue_op_key_init(&srv->send_key, sizeof(srv->send_key));

    status = pj_activesock_start_recvfrom(srv->asock, pool, 32, 0);
    if (status != PJ_SUCCESS) {
        pj_activesock_close(srv->asock);
        udp_echo_err("pj_activesock_start_recvfrom()", status);
        return status;
    }


    *p_srv = srv;
    return PJ_SUCCESS;
}

static void udp_echo_srv_destroy(struct udp_echo_srv *srv)
{
    pj_activesock_close(srv->asock);
}

/*******************************************************************
 * UDP ping pong test (send packet back and forth between two UDP echo
 * servers.
 */
/* was udp_ping_pong_test(void) */
static int activesock_test0(void)
{
    pj_ioqueue_t *ioqueue = NULL;
    pj_pool_t *pool = NULL;
    struct udp_echo_srv *srv1=NULL, *srv2=NULL;
    pj_bool_t need_send = PJ_TRUE;
    unsigned data = 0;
    int count, ret;
    pj_status_t status;

    pool = pj_pool_create(mem, "pingpong", 512, 512, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -10);

    PJ_TEST_SUCCESS(pj_ioqueue_create(pool, 4, &ioqueue), NULL, ERR(-20))
    PJ_TEST_SUCCESS(udp_echo_srv_create(pool, ioqueue, PJ_TRUE, &srv1), 
                    NULL, ERR(-30));
    PJ_TEST_SUCCESS(udp_echo_srv_create(pool, ioqueue, PJ_TRUE, &srv2),
                    NULL, ERR(-40));

    /* initiate the first send */
    for (count=0; count<1000; ++count) {
        unsigned last_rx1, last_rx2;
        unsigned i;

        if (need_send) {
            pj_str_t loopback;
            pj_sockaddr_in addr;
            pj_ssize_t sent;

            ++data;

            sent = sizeof(data);
            loopback = pj_str("127.0.0.1");
            pj_sockaddr_in_init(&addr, &loopback, srv2->port);
            status = pj_activesock_sendto(srv1->asock, &srv1->send_key,
                                          &data, &sent, 0,
                                          &addr, sizeof(addr));
            PJ_TEST_TRUE(status==PJ_SUCCESS || status==PJ_EPENDING,
                         "pj_activesock_sendto()", ERR(-50));

            need_send = PJ_FALSE;
        }

        last_rx1 = srv1->rx_cnt;
        last_rx2 = srv2->rx_cnt;

        for (i=0; i<10 && last_rx1 == srv1->rx_cnt && last_rx2 == srv2->rx_cnt; ++i) {
            pj_time_val delay = {0, 10};
#ifdef PJ_SYMBIAN
            PJ_UNUSED_ARG(delay);
            pj_symbianos_poll(-1, 100);
#else
            pj_ioqueue_poll(ioqueue, &delay);
#endif
        }

        PJ_TEST_EQ(srv1->rx_err_cnt+srv1->tx_err_cnt, 0, NULL, ERR(-60));
        PJ_TEST_EQ(srv2->rx_err_cnt+srv2->tx_err_cnt, 0, NULL, ERR(-62));
        PJ_TEST_TRUE(last_rx1 != srv1->rx_cnt || last_rx2 != srv2->rx_cnt,
                     "timeout/packets have been lost", ERR(-70));
    }

    ret = 0;

on_return:
    if (srv2)
        udp_echo_srv_destroy(srv2);
    if (srv1)
        udp_echo_srv_destroy(srv1);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);
    
    return ret;
}



#define SIGNATURE   0xdeadbeef
struct tcp_pkt
{
    pj_uint32_t signature;
    pj_uint32_t seq;
    char        fill[513];
};

struct tcp_state
{
    pj_bool_t   err;
    pj_bool_t   sent;
    pj_uint32_t next_recv_seq;
    pj_uint8_t  pkt[600];
};

struct send_key
{
    pj_ioqueue_op_key_t op_key;
};


static pj_bool_t tcp_on_data_read(pj_activesock_t *asock,
                                  void *data,
                                  pj_size_t size,
                                  pj_status_t status,
                                  pj_size_t *remainder)
{
    struct tcp_state *st = (struct tcp_state*) pj_activesock_get_user_data(asock);
    char *next = (char*) data;

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        PJ_LOG(1,("", "   err: status=%d", status));
        st->err = PJ_TRUE;
        return PJ_FALSE;
    }

    while (size >= sizeof(struct tcp_pkt)) {
        struct tcp_pkt *tcp_pkt = (struct tcp_pkt*) next;

        if (tcp_pkt->signature != SIGNATURE) {
            PJ_LOG(1,("", "   err: invalid signature at seq=%d", 
                          st->next_recv_seq));
            st->err = PJ_TRUE;
            return PJ_FALSE;
        }
        if (tcp_pkt->seq != st->next_recv_seq) {
            PJ_LOG(1,("", "   err: wrong sequence"));
            st->err = PJ_TRUE;
            return PJ_FALSE;
        }

        st->next_recv_seq++;
        next += sizeof(struct tcp_pkt);
        size -= sizeof(struct tcp_pkt);
    }

    if (size) {
        pj_memmove(data, next, size);
        *remainder = size;
    }

    return PJ_TRUE;
}

static pj_bool_t tcp_on_data_sent(pj_activesock_t *asock,
                                  pj_ioqueue_op_key_t *op_key,
                                  pj_ssize_t sent)
{
    struct tcp_state *st=(struct tcp_state*)pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(op_key);

    st->sent = 1;

    if (sent < 1) {
        st->err = PJ_TRUE;
        return PJ_FALSE;
    }

    return PJ_TRUE;
}

/* was tcp_perf_test() */
static int activesock_test1(void)
{
    enum { COUNT=10000 };
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_sock_t sock1=PJ_INVALID_SOCKET, sock2=PJ_INVALID_SOCKET;
    pj_activesock_t *asock1 = NULL, *asock2 = NULL;
    pj_activesock_cb cb;
    struct tcp_state *state1, *state2;
    unsigned i;
    int ret = 0;
    pj_status_t status = PJ_SUCCESS;

    pool = pj_pool_create(mem, "activesock_test1", 256, 256, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -10);

    PJ_TEST_SUCCESS( app_socketpair(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock1,
                                    &sock2),
                     NULL, ERR(-100));
    PJ_TEST_SUCCESS(pj_ioqueue_create(pool, 4, &ioqueue),
                    NULL, ERR(-110));

    pj_bzero(&cb, sizeof(cb));
    cb.on_data_read = &tcp_on_data_read;
    cb.on_data_sent = &tcp_on_data_sent;

    state1 = PJ_POOL_ZALLOC_T(pool, struct tcp_state);
    PJ_TEST_SUCCESS(pj_activesock_create(pool, sock1, pj_SOCK_STREAM(), 
                                         NULL, ioqueue, &cb, state1, &asock1),
                    NULL, ERR(-120));

    state2 = PJ_POOL_ZALLOC_T(pool, struct tcp_state);
    PJ_TEST_SUCCESS(pj_activesock_create(pool, sock2, pj_SOCK_STREAM(), NULL,
                                         ioqueue, &cb, state2, &asock2),
                    NULL, ERR(-130));
    PJ_TEST_SUCCESS(pj_activesock_start_read(asock1, pool, 1000, 0),
                    NULL, ERR(-140));

    /* Send packet as quickly as possible */
    for (i=0; i<COUNT && !state1->err && !state2->err; ++i) {
        struct tcp_pkt *pkt;
        struct send_key send_key[2], *op_key;
        pj_ssize_t len;

        pkt = (struct tcp_pkt*)state2->pkt;
        pkt->signature = SIGNATURE;
        pkt->seq = i;
        pj_memset(pkt->fill, 'a', sizeof(pkt->fill));

        op_key = &send_key[i%2];
        pj_ioqueue_op_key_init(&op_key->op_key, sizeof(*op_key));

        state2->sent = PJ_FALSE;
        len = sizeof(*pkt);
        status = pj_activesock_send(asock2, &op_key->op_key, pkt, &len, 0);
        if (status == PJ_EPENDING) {
            do {
#if PJ_SYMBIAN
                pj_symbianos_poll(-1, -1);
#else
                pj_ioqueue_poll(ioqueue, NULL);
#endif
            } while (!state2->sent);
        } else {
#if PJ_SYMBIAN
                /* The Symbian socket always returns PJ_SUCCESS for TCP send,
                 * eventhough the remote end hasn't received the data yet.
                 * If we continue sending, eventually send() will block,
                 * possibly because the send buffer is full. So we need to
                 * poll the ioqueue periodically, to let receiver gets the 
                 * data.
                 */
                pj_symbianos_poll(-1, 0);
#endif
                PJ_TEST_SUCCESS(status, "send error", ERR(-180));
                if (status == PJ_SUCCESS) {
                    PJ_TEST_EQ(len, sizeof(*pkt), 
                               "shouldn't report partial sent", ERR(-190));
                }
        }

#ifndef PJ_SYMBIAN
        for (;;) {
            pj_time_val timeout = {0, 10};
            if (pj_ioqueue_poll(ioqueue, &timeout) < 1)
                break;
        }
#endif

    }

    /* Wait until everything has been sent/received */
    if (state1->next_recv_seq < COUNT) {
#ifdef PJ_SYMBIAN
        while (pj_symbianos_poll(-1, 1000) == PJ_TRUE)
            ;
#else
        pj_time_val delay = {0, 100};
        while (pj_ioqueue_poll(ioqueue, &delay) > 0)
            ;
#endif
    }

    if (status == PJ_EPENDING)
        status = PJ_SUCCESS;

    /* this should not happen. non-success should have been dealt with */
    PJ_TEST_SUCCESS(status, NULL, ERR(-182));
    
    PJ_TEST_EQ(state1->err, 0, NULL, ERR(-183));
    PJ_TEST_EQ(state2->err, 0, NULL, ERR(-186));
    PJ_TEST_EQ(state1->next_recv_seq, COUNT, 
               "not all packets are received", ERR(-195));

on_return:
    if (asock2)
        pj_activesock_close(asock2);
    if (asock1)
        pj_activesock_close(asock1);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);

    return ret;
}

int activesock_test(void)
{
    int rc;
    if ((rc=activesock_test0()) != 0)
        return rc;

    if ((rc=activesock_test1()) != 0)
        return rc;

    return 0;
}

#else   /* INCLUDE_ACTIVESOCK_TEST */
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_active_sock_test;
#endif  /* INCLUDE_ACTIVESOCK_TEST */


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
#include "ssl_sock_test.h"


#if INCLUDE_SSLSOCK_TEST

/*
 * SSL send load test: blast many sends, verify all callbacks fire.
 * Uses small SO_SNDBUF to force async (PJ_EPENDING) send path.
 */
#define SEND_LOAD_COUNT     100
#define SEND_LOAD_PKT_LEN   512

struct send_load_state
{
    pj_pool_t          *pool;
    pj_ssl_sock_t      *accepted_ssock;
    pj_bool_t           is_server;
    pj_bool_t           echo;
    pj_status_t         err;
    pj_size_t           sent;
    pj_size_t           recv;
    pj_uint8_t          read_buf[8192];
    pj_bool_t           done;
    int                 pending_cnt;
    int                 sent_cb_cnt;
    int                 send_idx;
    pj_ioqueue_op_key_t op_keys[SEND_LOAD_COUNT];
    char                send_data[SEND_LOAD_PKT_LEN];
};

static pj_bool_t load_on_connect_complete(pj_ssl_sock_t *ssock,
                                          pj_status_t status)
{
    struct send_load_state *st = (struct send_load_state *)
                                 pj_ssl_sock_get_user_data(ssock);
    void *read_buf[1];
    int i;

    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    /* Start reading */
    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(ssock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    /* Blast sends — many rapid sends naturally trigger PJ_EPENDING
     * as the SSL/network buffers fill up.
     */
    for (i = 0; i < SEND_LOAD_COUNT; i++) {
        pj_ssize_t len = SEND_LOAD_PKT_LEN;

        status = pj_ssl_sock_send(ssock, &st->op_keys[i],
                                  st->send_data, &len, 0);
        if (status == PJ_EPENDING) {
            st->pending_cnt++;
        } else if (status == PJ_SUCCESS) {
            st->sent += len;
        } else {
            st->err = status;
            return PJ_FALSE;
        }
        st->send_idx++;
    }

    return PJ_TRUE;
}

static pj_bool_t load_on_accept_complete(pj_ssl_sock_t *ssock,
                                         pj_ssl_sock_t *newsock,
                                         const pj_sockaddr_t *src_addr,
                                         int src_addr_len,
                                         pj_status_t accept_status)
{
    struct send_load_state *parent_st = (struct send_load_state *)
                                        pj_ssl_sock_get_user_data(ssock);
    struct send_load_state *st;
    void *read_buf[1];
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (accept_status != PJ_SUCCESS)
        return PJ_FALSE;

    st = (struct send_load_state *)pj_pool_zalloc(parent_st->pool,
                                       sizeof(struct send_load_state));
    *st = *parent_st;
    pj_ssl_sock_set_user_data(newsock, st);

    /* Track accepted socket for cleanup */
    parent_st->accepted_ssock = newsock;

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(newsock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t load_on_data_read(pj_ssl_sock_t *ssock,
                                   void *data,
                                   pj_size_t size,
                                   pj_status_t status,
                                   pj_size_t *remainder)
{
    struct send_load_state *st = (struct send_load_state *)
                                  pj_ssl_sock_get_user_data(ssock);

    if (remainder)
        *remainder = 0;

    if (size > 0) {
        st->recv += size;

        /* Server echoes data back */
        if (st->echo) {
            pj_ssize_t sz = (pj_ssize_t)size;
            pj_status_t s;

            s = pj_ssl_sock_send(ssock, &st->op_keys[0], data, &sz, 0);
            if (s != PJ_SUCCESS && s != PJ_EPENDING) {
                st->err = s;
            }
        }

        /* Client: check if all echoed data received */
        if (!st->is_server) {
            pj_size_t expected = (pj_size_t)st->send_idx *
                                 SEND_LOAD_PKT_LEN;
            if (st->recv >= expected)
                st->done = PJ_TRUE;
        }
    }

    if (status != PJ_SUCCESS) {
        if (status == PJ_EEOF) {
            st->done = PJ_TRUE;
        } else {
            st->err = status;
        }
    }

    if (st->err != PJ_SUCCESS || st->done)
        return PJ_FALSE;

    return PJ_TRUE;
}

static pj_bool_t load_on_data_sent(pj_ssl_sock_t *ssock,
                                   pj_ioqueue_op_key_t *op_key,
                                   pj_ssize_t sent)
{
    struct send_load_state *st = (struct send_load_state *)
                                  pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(op_key);

    if (sent < 0) {
        st->err = (pj_status_t)-sent;
        return PJ_FALSE;
    }

    st->sent += sent;
    st->sent_cb_cnt++;
    return PJ_TRUE;
}

static int send_load_test(void)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t *ssock_cli = NULL;
    pj_ssl_sock_param param;
    struct send_load_state state_serv;
    struct send_load_state state_cli;
    pj_sockaddr addr, listen_addr;

    pj_status_t status;
    int i;

    pool = pj_pool_create(mem, "ssl_load", 4096, 4096, NULL);

    pj_bzero(&state_serv, sizeof(state_serv));
    pj_bzero(&state_cli, sizeof(state_cli));

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
        app_perror("...send_load_test: ioqueue create", status);
        goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...send_load_test: timer create", status);
        goto on_return;
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete2 = &load_on_accept_complete;
    param.cb.on_connect_complete = &load_on_connect_complete;
    param.cb.on_data_read = &load_on_data_read;
    param.cb.on_data_sent = &load_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.proto = PJ_SSL_SOCK_PROTO_TLS1_2;
    param.ciphers_num = 0;

    {
        pj_str_t tmp_st;
        pj_sockaddr_init(PJ_AF_INET, &addr,
                         pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* Fill send data with pattern */
    for (i = 0; i < SEND_LOAD_PKT_LEN; i++)
        state_cli.send_data[i] = (char)(i & 0xFF);

    /* SERVER */
    state_serv.pool = pool;
    state_serv.echo = PJ_TRUE;
    state_serv.is_server = PJ_TRUE;
    param.user_data = &state_serv;
    param.require_client_cert = PJ_FALSE;

    listen_addr = addr;
    status = ssl_test_create_server(pool, &param, "send_load_test",
                                    &ssock_serv, &listen_addr);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* CLIENT */
    param.user_data = &state_cli;
    param.require_client_cert = PJ_FALSE;

    state_cli.pool = pool;
    state_cli.echo = PJ_FALSE;
    state_cli.is_server = PJ_FALSE;

    status = pj_ssl_sock_create(pool, &param, &ssock_cli);
    if (status != PJ_SUCCESS) {
        app_perror("...send_load_test: client create", status);
        goto on_return;
    }

    status = pj_ssl_sock_start_connect(ssock_cli, pool, &addr,
                                       &listen_addr,
                                       pj_sockaddr_get_len(&addr));
    if (status == PJ_SUCCESS) {
        load_on_connect_complete(ssock_cli, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
        app_perror("...send_load_test: connect", status);
        goto on_return;
    }

    /* Poll until done or error */
    {
        pj_timestamp t_start, t_now;
        pj_uint32_t elapsed;

        pj_get_timestamp(&t_start);
        while (!state_cli.err && !state_cli.done) {
            pj_time_val delay = {0, 100};
            pj_ioqueue_poll(ioqueue, &delay);
            pj_timer_heap_poll(timer, NULL);

            pj_get_timestamp(&t_now);
            elapsed = pj_elapsed_msec(&t_start, &t_now);
            if (elapsed > 30000) {
                PJ_LOG(1, ("", "...send_load_test TIMEOUT after 30s"));
                status = PJ_ETIMEDOUT;
                goto on_return;
            }
        }
    }

    if (state_cli.err) {
        status = state_cli.err;
        app_perror("...send_load_test client error", status);
        goto on_return;
    }

    /* Verify results */
    PJ_LOG(3, ("", "...send_load_test: sent=%lu, recv=%lu, "
               "pending=%d, sent_cb=%d",
               (unsigned long)state_cli.sent,
               (unsigned long)state_cli.recv,
               state_cli.pending_cnt,
               state_cli.sent_cb_cnt));

    if (state_cli.pending_cnt == 0) {
        PJ_LOG(3, ("", "...NOTE: all sends completed synchronously, "
                   "async path NOT tested. Set PJ_IOQUEUE_FAST_TRACK=0 "
                   "in config_site.h to force async path."));
    }

    if (state_cli.sent_cb_cnt != state_cli.pending_cnt) {
        PJ_LOG(1, ("", "...ERROR: sent callback count (%d) != "
                   "pending count (%d)",
                   state_cli.sent_cb_cnt, state_cli.pending_cnt));
        status = PJ_EBUG;
        goto on_return;
    }

    if (state_cli.sent != (pj_size_t)SEND_LOAD_COUNT * SEND_LOAD_PKT_LEN) {
        PJ_LOG(1, ("", "...ERROR: total sent (%lu) != expected (%lu)",
                   (unsigned long)state_cli.sent,
                   (unsigned long)(SEND_LOAD_COUNT * SEND_LOAD_PKT_LEN)));
        status = PJ_EBUG;
        goto on_return;
    }

    status = PJ_SUCCESS;

on_return:
    if (ssock_cli)
        pj_ssl_sock_close(ssock_cli);
    if (state_serv.accepted_ssock)
        pj_ssl_sock_close(state_serv.accepted_ssock);
    if (ssock_serv)
        pj_ssl_sock_close(ssock_serv);

    /* Poll to drain pending events after close */
    if (ioqueue) {
        pj_time_val delay = {0, 500};
        int n = 50;
        while (n-- > 0 && pj_ioqueue_poll(ioqueue, &delay) > 0)
            ;
    }

    if (timer)
        pj_timer_heap_destroy(timer);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);

    return (status == PJ_SUCCESS) ? 0 : -1;
}

/*
 * Close under pending sends test: blast sends then immediately close socket.
 * Verifies no crash or use-after-free.
 */
struct close_pending_state
{
    pj_pool_t          *pool;
    pj_ssl_sock_t      *accepted_ssock;
    pj_bool_t           is_server;
    pj_bool_t           echo;
    pj_status_t         err;
    pj_size_t           sent;
    pj_size_t           recv;
    pj_uint8_t          read_buf[8192];
    pj_bool_t           done;
    int                 pending_cnt;
    int                 sent_cb_cnt;
    int                 send_idx;
    pj_ioqueue_op_key_t op_keys[SEND_LOAD_COUNT];
    char                send_data[SEND_LOAD_PKT_LEN];
    pj_bool_t           blast_done;
};

static pj_bool_t cp_on_connect_complete(pj_ssl_sock_t *ssock,
                                        pj_status_t status)
{
    struct close_pending_state *st = (struct close_pending_state *)
                                     pj_ssl_sock_get_user_data(ssock);
    void *read_buf[1];
    pj_ssize_t len;
    int i;

    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(ssock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    /* Blast sends */
    for (i = 0; i < SEND_LOAD_COUNT; i++) {
        len = SEND_LOAD_PKT_LEN;
        status = pj_ssl_sock_send(ssock, &st->op_keys[i],
                                  st->send_data, &len, 0);
        if (status == PJ_EPENDING) {
            st->pending_cnt++;
        } else if (status == PJ_SUCCESS) {
            st->sent += len;
        } else {
            /* Send error during blast, stop but don't fail test */
            break;
        }
        st->send_idx++;
    }

    st->blast_done = PJ_TRUE;
    return PJ_TRUE;
}

static pj_bool_t cp_on_data_read(pj_ssl_sock_t *ssock,
                                 void *data,
                                 pj_size_t size,
                                 pj_status_t status,
                                 pj_size_t *remainder)
{
    struct close_pending_state *st = (struct close_pending_state *)
                                     pj_ssl_sock_get_user_data(ssock);

    PJ_UNUSED_ARG(data);

    if (remainder)
        *remainder = 0;

    if (size > 0)
        st->recv += size;

    /* Server: just consume data, no echo needed for this test */
    if (status != PJ_SUCCESS && status != PJ_EEOF) {
        st->err = status;
        return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t cp_on_data_sent(pj_ssl_sock_t *ssock,
                                 pj_ioqueue_op_key_t *op_key,
                                 pj_ssize_t sent)
{
    struct close_pending_state *st = (struct close_pending_state *)
                                     pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(op_key);

    /* Tolerate errors — socket may be closing under us */
    if (sent > 0)
        st->sent += sent;
    st->sent_cb_cnt++;

    return PJ_TRUE;
}

static int close_pending_test(void)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t *ssock_cli = NULL;
    pj_ssl_sock_param param;
    struct close_pending_state state_serv;
    struct close_pending_state state_cli;
    pj_sockaddr addr, listen_addr;

    pj_status_t status;
    int i;

    pool = pj_pool_create(mem, "ssl_closep", 8192, 4096, NULL);

    pj_bzero(&state_serv, sizeof(state_serv));
    pj_bzero(&state_cli, sizeof(state_cli));

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
        app_perror("...close_pending_test: ioqueue create", status);
        goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...close_pending_test: timer create", status);
        goto on_return;
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete2 = &load_on_accept_complete;
    param.cb.on_connect_complete = &cp_on_connect_complete;
    param.cb.on_data_read = &cp_on_data_read;
    param.cb.on_data_sent = &cp_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.proto = PJ_SSL_SOCK_PROTO_TLS1_2;
    param.ciphers_num = 0;

    {
        pj_str_t tmp_st;
        pj_sockaddr_init(PJ_AF_INET, &addr,
                         pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    for (i = 0; i < SEND_LOAD_PKT_LEN; i++)
        state_cli.send_data[i] = (char)(i & 0xFF);

    /* SERVER */
    state_serv.pool = pool;
    state_serv.is_server = PJ_TRUE;
    param.user_data = &state_serv;
    param.require_client_cert = PJ_FALSE;

    listen_addr = addr;
    status = ssl_test_create_server(pool, &param, "close_pending_test",
                                    &ssock_serv, &listen_addr);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* CLIENT */
    state_cli.pool = pool;
    state_cli.is_server = PJ_FALSE;
    param.user_data = &state_cli;

    status = pj_ssl_sock_create(pool, &param, &ssock_cli);
    if (status != PJ_SUCCESS) {
        app_perror("...close_pending_test: client create", status);
        goto on_return;
    }

    status = pj_ssl_sock_start_connect(ssock_cli, pool, &addr,
                                       &listen_addr,
                                       pj_sockaddr_get_len(&addr));
    if (status == PJ_SUCCESS) {
        cp_on_connect_complete(ssock_cli, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
        app_perror("...close_pending_test: connect", status);
        goto on_return;
    }

    /* Poll until blast is done */
    {
        pj_timestamp t_start, t_now;
        pj_uint32_t elapsed;

        pj_get_timestamp(&t_start);
        while (!state_cli.blast_done && !state_cli.err) {
            pj_time_val delay = {0, 100};
            pj_ioqueue_poll(ioqueue, &delay);
            pj_timer_heap_poll(timer, NULL);

            pj_get_timestamp(&t_now);
            elapsed = pj_elapsed_msec(&t_start, &t_now);
            if (elapsed > 30000) {
                PJ_LOG(1, ("", "...close_pending_test TIMEOUT"));
                status = PJ_ETIMEDOUT;
                goto on_return;
            }
        }
    }

    /* Close client while sends may still be pending */
    PJ_LOG(3, ("", "...close_pending_test: closing client with "
               "pending=%d", state_cli.pending_cnt));
    pj_ssl_sock_close(ssock_cli);
    ssock_cli = NULL;

    /* Poll briefly to let server-side and async cleanup drain */
    {
        pj_time_val delay = {0, 500};
        int n = 20;
        while (n-- > 0)
            pj_ioqueue_poll(ioqueue, &delay);
    }

    PJ_LOG(3, ("", "...close_pending_test: completed (no crash)"));
    status = PJ_SUCCESS;

on_return:
    if (ssock_cli)
        pj_ssl_sock_close(ssock_cli);
    if (state_serv.accepted_ssock)
        pj_ssl_sock_close(state_serv.accepted_ssock);
    if (ssock_serv)
        pj_ssl_sock_close(ssock_serv);

    /* Poll to drain pending events after close */
    if (ioqueue) {
        pj_time_val delay = {0, 500};
        int n = 50;
        while (n-- > 0 && pj_ioqueue_poll(ioqueue, &delay) > 0)
            ;
    }

    if (timer)
        pj_timer_heap_destroy(timer);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);

    return (status == PJ_SUCCESS) ? 0 : -1;
}


/*
 * Bidirectional simultaneous load test: both sides send independent data.
 */
#define BIDIR_SEND_COUNT    100
#define BIDIR_PKT_LEN      1024

struct bidir_state
{
    pj_pool_t          *pool;
    pj_ssl_sock_t      *accepted_ssock;
    pj_bool_t           is_server;
    pj_status_t         err;
    pj_size_t           sent;
    pj_size_t           recv;
    pj_uint8_t          read_buf[8192];
    pj_bool_t           done;
    int                 pending_cnt;
    int                 sent_cb_cnt;
    int                 send_idx;
    pj_ioqueue_op_key_t op_keys[BIDIR_SEND_COUNT];
    char                send_data[BIDIR_PKT_LEN];
    pj_size_t           expected_recv;
};

static pj_bool_t bidir_blast_sends(pj_ssl_sock_t *ssock,
                                   struct bidir_state *st)
{
    int i;

    for (i = st->send_idx; i < BIDIR_SEND_COUNT; i++) {
        pj_ssize_t len = BIDIR_PKT_LEN;
        pj_status_t s;

        s = pj_ssl_sock_send(ssock, &st->op_keys[i],
                             st->send_data, &len, 0);
        if (s == PJ_EPENDING) {
            st->pending_cnt++;
        } else if (s == PJ_SUCCESS) {
            st->sent += len;
        } else {
            st->err = s;
            return PJ_FALSE;
        }
        st->send_idx++;
    }
    return PJ_TRUE;
}

static pj_bool_t bidir_on_connect_complete(pj_ssl_sock_t *ssock,
                                           pj_status_t status)
{
    struct bidir_state *st = (struct bidir_state *)
                              pj_ssl_sock_get_user_data(ssock);
    void *read_buf[1];

    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(ssock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    return bidir_blast_sends(ssock, st);
}

static pj_bool_t bidir_on_accept_complete(pj_ssl_sock_t *ssock,
                                          pj_ssl_sock_t *newsock,
                                          const pj_sockaddr_t *src_addr,
                                          int src_addr_len,
                                          pj_status_t accept_status)
{
    struct bidir_state *parent_st = (struct bidir_state *)
                                    pj_ssl_sock_get_user_data(ssock);
    struct bidir_state *st;
    void *read_buf[1];
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (accept_status != PJ_SUCCESS)
        return PJ_FALSE;

    st = (struct bidir_state *)pj_pool_zalloc(parent_st->pool,
                                              sizeof(struct bidir_state));
    *st = *parent_st;
    st->send_idx = 0;
    st->pending_cnt = 0;
    st->sent_cb_cnt = 0;
    st->sent = 0;
    st->recv = 0;
    st->done = PJ_FALSE;
    st->err = PJ_SUCCESS;
    pj_ssl_sock_set_user_data(newsock, st);

    /* Track accepted socket for cleanup */
    parent_st->accepted_ssock = newsock;

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(newsock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    /* Server also blast-sends its own data */
    return bidir_blast_sends(newsock, st);
}

static pj_bool_t bidir_on_data_read(pj_ssl_sock_t *ssock,
                                    void *data,
                                    pj_size_t size,
                                    pj_status_t status,
                                    pj_size_t *remainder)
{
    struct bidir_state *st = (struct bidir_state *)
                              pj_ssl_sock_get_user_data(ssock);

    PJ_UNUSED_ARG(data);

    if (remainder)
        *remainder = 0;

    if (size > 0) {
        st->recv += size;
        if (st->recv >= st->expected_recv)
            st->done = PJ_TRUE;
    }

    if (status != PJ_SUCCESS) {
        if (status == PJ_EEOF)
            st->done = PJ_TRUE;
        else
            st->err = status;
    }

    if (st->err != PJ_SUCCESS || st->done)
        return PJ_FALSE;

    return PJ_TRUE;
}

static pj_bool_t bidir_on_data_sent(pj_ssl_sock_t *ssock,
                                    pj_ioqueue_op_key_t *op_key,
                                    pj_ssize_t sent)
{
    struct bidir_state *st = (struct bidir_state *)
                              pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(op_key);

    if (sent < 0) {
        st->err = (pj_status_t)-sent;
        return PJ_FALSE;
    }

    st->sent += sent;
    st->sent_cb_cnt++;
    return PJ_TRUE;
}

static int bidir_test(void)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t *ssock_cli = NULL;
    pj_ssl_sock_param param;
    struct bidir_state state_serv;
    struct bidir_state state_cli;
    pj_sockaddr addr, listen_addr;

    pj_status_t status;
    int i;

    pool = pj_pool_create(mem, "ssl_bidir", 8192, 4096, NULL);

    pj_bzero(&state_serv, sizeof(state_serv));
    pj_bzero(&state_cli, sizeof(state_cli));

    status = pj_ioqueue_create(pool, 4, &ioqueue);
    if (status != PJ_SUCCESS) {
        app_perror("...bidir_test: ioqueue create", status);
        goto on_return;
    }

    status = pj_timer_heap_create(pool, 4, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...bidir_test: timer create", status);
        goto on_return;
    }

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete2 = &bidir_on_accept_complete;
    param.cb.on_connect_complete = &bidir_on_connect_complete;
    param.cb.on_data_read = &bidir_on_data_read;
    param.cb.on_data_sent = &bidir_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.proto = PJ_SSL_SOCK_PROTO_TLS1_2;
    param.ciphers_num = 0;

    {
        pj_str_t tmp_st;
        pj_sockaddr_init(PJ_AF_INET, &addr,
                         pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* Fill send data with unique patterns per side */
    for (i = 0; i < BIDIR_PKT_LEN; i++) {
        state_serv.send_data[i] = (char)(0xAA ^ (i & 0xFF));
        state_cli.send_data[i] = (char)(0xBB ^ (i & 0xFF));
    }

    /* SERVER */
    state_serv.pool = pool;
    state_serv.is_server = PJ_TRUE;
    state_serv.expected_recv = BIDIR_SEND_COUNT * BIDIR_PKT_LEN;
    param.user_data = &state_serv;
    param.require_client_cert = PJ_FALSE;

    listen_addr = addr;
    status = ssl_test_create_server(pool, &param, "bidir_test",
                                    &ssock_serv, &listen_addr);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* CLIENT */
    state_cli.pool = pool;
    state_cli.is_server = PJ_FALSE;
    state_cli.expected_recv = BIDIR_SEND_COUNT * BIDIR_PKT_LEN;
    param.user_data = &state_cli;

    status = pj_ssl_sock_create(pool, &param, &ssock_cli);
    if (status != PJ_SUCCESS) {
        app_perror("...bidir_test: client create", status);
        goto on_return;
    }

    status = pj_ssl_sock_start_connect(ssock_cli, pool, &addr,
                                       &listen_addr,
                                       pj_sockaddr_get_len(&addr));
    if (status == PJ_SUCCESS) {
        bidir_on_connect_complete(ssock_cli, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
        app_perror("...bidir_test: connect", status);
        goto on_return;
    }

    /* Poll until client done */
    {
        pj_timestamp t_start, t_now;
        pj_uint32_t elapsed;

        pj_get_timestamp(&t_start);
        while (!state_cli.err && !state_cli.done) {
            pj_time_val delay = {0, 100};
            pj_ioqueue_poll(ioqueue, &delay);
            pj_timer_heap_poll(timer, NULL);

            pj_get_timestamp(&t_now);
            elapsed = pj_elapsed_msec(&t_start, &t_now);
            if (elapsed > 30000) {
                PJ_LOG(1, ("", "...bidir_test TIMEOUT after 30s"));
                status = PJ_ETIMEDOUT;
                goto on_return;
            }
        }
    }

    if (state_cli.err) {
        status = state_cli.err;
        app_perror("...bidir_test client error", status);
        goto on_return;
    }

    PJ_LOG(3, ("", "...bidir_test: cli sent=%lu recv=%lu "
               "pending=%d sent_cb=%d",
               (unsigned long)state_cli.sent,
               (unsigned long)state_cli.recv,
               state_cli.pending_cnt,
               state_cli.sent_cb_cnt));

    if (state_cli.recv < state_cli.expected_recv) {
        PJ_LOG(1, ("", "...bidir_test: recv=%lu < expected=%lu",
                   (unsigned long)state_cli.recv,
                   (unsigned long)state_cli.expected_recv));
        status = PJ_EINVAL;
        goto on_return;
    }

    if (state_cli.pending_cnt != state_cli.sent_cb_cnt) {
        PJ_LOG(1, ("", "...bidir_test: cli pending=%d != sent_cb=%d",
                   state_cli.pending_cnt, state_cli.sent_cb_cnt));
        status = PJ_EBUG;
        goto on_return;
    }

    status = PJ_SUCCESS;

on_return:
    if (ssock_cli)
        pj_ssl_sock_close(ssock_cli);
    if (state_serv.accepted_ssock)
        pj_ssl_sock_close(state_serv.accepted_ssock);
    if (ssock_serv)
        pj_ssl_sock_close(ssock_serv);

    /* Poll to drain pending events after close */
    if (ioqueue) {
        pj_time_val delay = {0, 500};
        int n = 50;
        while (n-- > 0 && pj_ioqueue_poll(ioqueue, &delay) > 0)
            ;
    }

    if (timer)
        pj_timer_heap_destroy(timer);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);

    return (status == PJ_SUCCESS) ? 0 : -1;
}


/*
 * Multi-threaded send load test: multiple clients with worker threads.
 * This is the closest simulation to production (multiple SIP registrations
 * over TLS with worker threads polling ioqueue).
 */
#if PJ_HAS_THREADS

#define MT_WORKER_THREADS   3
#define MT_CLIENTS          3
#define MT_SEND_COUNT       100
#define MT_SEND_PKT_LEN    512

struct mt_state
{
    pj_pool_t          *pool;
    pj_ssl_sock_t     **accepted_arr;
    int                 accepted_cnt;
    pj_bool_t           is_server;
    pj_bool_t           echo;
    pj_status_t         err;
    pj_size_t           sent;
    pj_size_t           recv;
    pj_uint8_t          read_buf[8192];
    pj_bool_t           done;
    int                 pending_cnt;
    int                 sent_cb_cnt;
    int                 send_idx;
    pj_ioqueue_op_key_t op_keys[MT_SEND_COUNT];
    char                send_data[MT_SEND_PKT_LEN];
};

struct mt_test_ctx
{
    pj_ioqueue_t       *ioqueue;
    pj_timer_heap_t    *timer;
    pj_bool_t           quit_flag;
    pj_atomic_t        *clients_done;
};

static pj_bool_t mt_on_connect_complete(pj_ssl_sock_t *ssock,
                                        pj_status_t status)
{
    struct mt_state *st = (struct mt_state *)
                           pj_ssl_sock_get_user_data(ssock);
    void *read_buf[1];
    pj_ssize_t len;
    int i;

    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(ssock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    for (i = 0; i < MT_SEND_COUNT; i++) {
        len = MT_SEND_PKT_LEN;
        status = pj_ssl_sock_send(ssock, &st->op_keys[i],
                                  st->send_data, &len, 0);
        if (status == PJ_EPENDING) {
            st->pending_cnt++;
        } else if (status == PJ_SUCCESS) {
            st->sent += len;
        } else {
            st->err = status;
            return PJ_FALSE;
        }
        st->send_idx++;
    }

    return PJ_TRUE;
}

static pj_bool_t mt_on_accept_complete(pj_ssl_sock_t *ssock,
                                       pj_ssl_sock_t *newsock,
                                       const pj_sockaddr_t *src_addr,
                                       int src_addr_len,
                                       pj_status_t accept_status)
{
    struct mt_state *parent_st = (struct mt_state *)
                                 pj_ssl_sock_get_user_data(ssock);
    struct mt_state *st;
    void *read_buf[1];
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (accept_status != PJ_SUCCESS)
        return PJ_FALSE;

    st = (struct mt_state *)pj_pool_zalloc(parent_st->pool,
                                           sizeof(struct mt_state));
    *st = *parent_st;
    st->sent = 0;
    st->recv = 0;
    st->done = PJ_FALSE;
    st->err = PJ_SUCCESS;
    pj_ssl_sock_set_user_data(newsock, st);

    /* Track accepted socket for cleanup */
    if (parent_st->accepted_arr &&
        parent_st->accepted_cnt < MT_CLIENTS)
    {
        parent_st->accepted_arr[parent_st->accepted_cnt++] = newsock;
    }

    read_buf[0] = st->read_buf;
    status = pj_ssl_sock_start_read2(newsock, st->pool,
                                     sizeof(st->read_buf),
                                     (void **)read_buf, 0);
    if (status != PJ_SUCCESS) {
        st->err = status;
        return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_bool_t mt_on_data_read(pj_ssl_sock_t *ssock,
                                 void *data,
                                 pj_size_t size,
                                 pj_status_t status,
                                 pj_size_t *remainder)
{
    struct mt_state *st = (struct mt_state *)
                           pj_ssl_sock_get_user_data(ssock);

    if (remainder)
        *remainder = 0;

    if (size > 0) {
        st->recv += size;

        /* Server echoes data back */
        if (st->echo) {
            pj_ssize_t sz = (pj_ssize_t)size;
            pj_status_t s;

            s = pj_ssl_sock_send(ssock, &st->op_keys[0], data, &sz, 0);
            if (s != PJ_SUCCESS && s != PJ_EPENDING)
                st->err = s;
        }

        /* Client: check completion */
        if (!st->is_server) {
            pj_size_t expected = (pj_size_t)MT_SEND_COUNT * MT_SEND_PKT_LEN;
            if (st->recv >= expected)
                st->done = PJ_TRUE;
        }
    }

    if (status != PJ_SUCCESS) {
        if (status == PJ_EEOF)
            st->done = PJ_TRUE;
        else
            st->err = status;
    }

    if (st->err != PJ_SUCCESS || st->done)
        return PJ_FALSE;

    return PJ_TRUE;
}

static pj_bool_t mt_on_data_sent(pj_ssl_sock_t *ssock,
                                 pj_ioqueue_op_key_t *op_key,
                                 pj_ssize_t sent)
{
    struct mt_state *st = (struct mt_state *)
                           pj_ssl_sock_get_user_data(ssock);
    PJ_UNUSED_ARG(op_key);

    if (sent < 0) {
        st->err = (pj_status_t)-sent;
        return PJ_FALSE;
    }

    st->sent += sent;
    st->sent_cb_cnt++;
    return PJ_TRUE;
}

static int mt_worker_proc(void *arg)
{
    struct mt_test_ctx *ctx = (struct mt_test_ctx *)arg;

    while (!ctx->quit_flag) {
        pj_time_val delay = {0, 20};
        pj_ioqueue_poll(ctx->ioqueue, &delay);
        pj_timer_heap_poll(ctx->timer, NULL);
    }

    return 0;
}

static int mt_send_load_test(void)
{
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_ssl_sock_t *ssock_serv = NULL;
    pj_ssl_sock_t *ssock_cli[MT_CLIENTS];
    pj_ssl_sock_t *ssock_accepted[MT_CLIENTS];
    pj_thread_t *threads[MT_WORKER_THREADS];
    pj_ssl_sock_param param;
    struct mt_state state_serv;
    struct mt_state state_cli[MT_CLIENTS];
    struct mt_test_ctx ctx;
    pj_sockaddr addr, listen_addr;

    pj_status_t status;
    int i;

    pj_bzero(ssock_cli, sizeof(ssock_cli));
    pj_bzero(ssock_accepted, sizeof(ssock_accepted));
    pj_bzero(threads, sizeof(threads));
    pj_bzero(&ctx, sizeof(ctx));

    pool = pj_pool_create(mem, "ssl_mt", 32000, 4096, NULL);

    pj_bzero(&state_serv, sizeof(state_serv));
    pj_bzero(state_cli, sizeof(state_cli));

    status = pj_ioqueue_create(pool, 4 + MT_CLIENTS * 2, &ioqueue);
    if (status != PJ_SUCCESS) {
        app_perror("...mt_send_load_test: ioqueue create", status);
        goto on_return;
    }

    status = pj_timer_heap_create(pool, 4 + MT_CLIENTS * 2, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...mt_send_load_test: timer create", status);
        goto on_return;
    }

    ctx.ioqueue = ioqueue;
    ctx.timer = timer;
    ctx.quit_flag = PJ_FALSE;

    pj_ssl_sock_param_default(&param);
    param.cb.on_accept_complete2 = &mt_on_accept_complete;
    param.cb.on_connect_complete = &mt_on_connect_complete;
    param.cb.on_data_read = &mt_on_data_read;
    param.cb.on_data_sent = &mt_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.proto = PJ_SSL_SOCK_PROTO_TLS1_2;
    param.ciphers_num = 0;

    {
        pj_str_t tmp_st;
        pj_sockaddr_init(PJ_AF_INET, &addr,
                         pj_strset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* SERVER */
    state_serv.pool = pool;
    state_serv.echo = PJ_TRUE;
    state_serv.is_server = PJ_TRUE;
    state_serv.accepted_arr = ssock_accepted;
    state_serv.accepted_cnt = 0;
    param.user_data = &state_serv;
    param.require_client_cert = PJ_FALSE;

    listen_addr = addr;
    status = ssl_test_create_server(pool, &param, "mt_send_load_test",
                                    &ssock_serv, &listen_addr);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Start worker threads */
    for (i = 0; i < MT_WORKER_THREADS; i++) {
        status = pj_thread_create(pool, "ssl_mt_w", &mt_worker_proc,
                                  &ctx, 0, 0, &threads[i]);
        if (status != PJ_SUCCESS) {
            app_perror("...mt_send_load_test: thread create", status);
            goto on_return;
        }
    }

    /* Create and connect clients */
    for (i = 0; i < MT_CLIENTS; i++) {
        int k;

        state_cli[i].pool = pool;
        state_cli[i].is_server = PJ_FALSE;
        for (k = 0; k < MT_SEND_PKT_LEN; k++)
            state_cli[i].send_data[k] = (char)((i + k) & 0xFF);

        param.user_data = &state_cli[i];

        status = pj_ssl_sock_create(pool, &param, &ssock_cli[i]);
        if (status != PJ_SUCCESS) {
            app_perror("...mt_send_load_test: client create", status);
            goto on_return;
        }

        status = pj_ssl_sock_start_connect(ssock_cli[i], pool, &addr,
                                           &listen_addr,
                                           pj_sockaddr_get_len(&addr));
        if (status == PJ_SUCCESS) {
            mt_on_connect_complete(ssock_cli[i], PJ_SUCCESS);
        } else if (status != PJ_EPENDING) {
            app_perror("...mt_send_load_test: connect", status);
            goto on_return;
        }
    }

    /* Main thread also polls */
    {
        pj_timestamp t_start, t_now;
        pj_uint32_t elapsed;
        int all_done;

        pj_get_timestamp(&t_start);
        for (;;) {
            pj_time_val delay = {0, 100};
            pj_ioqueue_poll(ioqueue, &delay);
            pj_timer_heap_poll(timer, NULL);

            all_done = PJ_TRUE;
            for (i = 0; i < MT_CLIENTS; i++) {
                if (!state_cli[i].done && !state_cli[i].err) {
                    all_done = PJ_FALSE;
                    break;
                }
            }
            if (all_done)
                break;

            pj_get_timestamp(&t_now);
            elapsed = pj_elapsed_msec(&t_start, &t_now);
            if (elapsed > 60000) {
                PJ_LOG(1, ("", "...mt_send_load_test TIMEOUT after 60s"));
                status = PJ_ETIMEDOUT;
                goto on_return;
            }
        }
    }

    /* Stop workers */
    ctx.quit_flag = PJ_TRUE;
    for (i = 0; i < MT_WORKER_THREADS; i++) {
        if (threads[i]) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
            threads[i] = NULL;
        }
    }

    /* Verify */
    for (i = 0; i < MT_CLIENTS; i++) {
        pj_size_t expected = (pj_size_t)MT_SEND_COUNT * MT_SEND_PKT_LEN;

        if (state_cli[i].err) {
            PJ_LOG(1, ("", "...mt_send_load_test: client %d error", i));
            status = state_cli[i].err;
            app_perror("...mt_send_load_test: client error", status);
            goto on_return;
        }

        PJ_LOG(3, ("", "...mt_send_load_test: cli[%d] sent=%lu recv=%lu "
                   "pending=%d sent_cb=%d",
                   i,
                   (unsigned long)state_cli[i].sent,
                   (unsigned long)state_cli[i].recv,
                   state_cli[i].pending_cnt,
                   state_cli[i].sent_cb_cnt));

        if (state_cli[i].pending_cnt != state_cli[i].sent_cb_cnt) {
            PJ_LOG(1, ("", "...mt_send_load_test: cli[%d] pending=%d "
                       "!= sent_cb=%d", i,
                       state_cli[i].pending_cnt,
                       state_cli[i].sent_cb_cnt));
            status = PJ_EBUG;
            goto on_return;
        }

        if (state_cli[i].sent < expected) {
            PJ_LOG(1, ("", "...mt_send_load_test: cli[%d] sent=%lu "
                       "< expected=%lu", i,
                       (unsigned long)state_cli[i].sent,
                       (unsigned long)expected));
            status = PJ_EBUG;
            goto on_return;
        }
    }

    status = PJ_SUCCESS;

on_return:
    ctx.quit_flag = PJ_TRUE;
    for (i = 0; i < MT_WORKER_THREADS; i++) {
        if (threads[i]) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }
    }

    for (i = 0; i < MT_CLIENTS; i++) {
        if (ssock_cli[i])
            pj_ssl_sock_close(ssock_cli[i]);
    }
    for (i = 0; i < state_serv.accepted_cnt; i++) {
        if (ssock_accepted[i])
            pj_ssl_sock_close(ssock_accepted[i]);
    }
    if (ssock_serv)
        pj_ssl_sock_close(ssock_serv);

    /* Poll to drain pending events after close */
    if (ioqueue) {
        pj_time_val delay = {0, 500};
        int n = 50;
        while (n-- > 0 && pj_ioqueue_poll(ioqueue, &delay) > 0)
            ;
    }

    if (timer)
        pj_timer_heap_destroy(timer);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (pool)
        pj_pool_release(pool);

    return (status == PJ_SUCCESS) ? 0 : -1;
}

#endif /* PJ_HAS_THREADS */


int ssl_sock_stress_test(void)
{
    int ret;

    PJ_LOG(3,("", "..send load test"));
    ret = send_load_test();
    if (ret != 0)
        return ret;

    PJ_LOG(3,("", "..close under pending sends test"));
    ret = close_pending_test();
    if (ret != 0)
        return ret;

    PJ_LOG(3,("", "..bidirectional simultaneous load test"));
    ret = bidir_test();
    if (ret != 0)
        return ret;

#if PJ_HAS_THREADS
    PJ_LOG(3,("", "..multi-threaded send load test"));
    ret = mt_send_load_test();
    if (ret != 0)
        return ret;
#endif

    return 0;
}

#else   /* INCLUDE_SSLSOCK_TEST */
int dummy_ssl_sock_stress_test;
#endif  /* INCLUDE_SSLSOCK_TEST */

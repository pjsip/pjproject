/*
 * Copyright (C) 2008-2022 Teluu Inc. (http://www.teluu.com)
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

#define THIS_FILE       "ioq_stress_test.c"
#define MAX_THREADS     16
#define TRACE(log)      PJ_LOG(3,log)
#define MAX_ASYNC       16

#define RETCODE_CONNECT_FAILED  650

/* index in socks[], keys[] array */
#define SERVER  0
#define CLIENT  1

/* epoll errors that can be ignored */
#define IGNORE_ERROR(e) (((e)==PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)))

typedef struct test_desc test_desc;

/* op_key user data */
typedef union op_key_user_data {
    struct {
        int side;           /* CLIENT or SERVER */
        pj_status_t status; /* PJ_SUCCESS: idle, PJ_EPENDING: pending, other: error */
        unsigned count;     /* how many times were used */
    } common;

    struct {
        int side;
        pj_status_t status;
        unsigned count;

        pj_sock_t new_sock;
        pj_ioqueue_op_key_t recv_op;
        unsigned *recv_buf;
    } server;

    struct {
        int side;
        pj_status_t status;
        unsigned count;

        pj_ioqueue_op_key_t connect_op;
        pj_ioqueue_op_key_t send_op;
        unsigned *send_buf;
    } client;
} op_key_user_data;


/* Description for a single test. This description contains two parts:
 * - configuration
 * - run-time state
 *
 * A single test contains an ioqueue, and (mostly) of a pair of socket:
 * server and client. (when TCP is used, a listening socket will be created
 * too).
 *
 * Each socket (server or client) will initiate MAX_ASYNC simultaneous
 * send() and recev() operations.
 *
 * One single pass of a test completes when the designated number of "packets"
 * have been exchanged by the client/server (cfg.rx_cnt).
 *
 * Then the test may be repeated "cfg.repeat" times, using the same ioqueue but
 * different set of sockets, to test ioqueue key recycling mechanism.
 */
struct test_desc
{
    struct {
        const char *title;
        int expected_ret_code;
        int max_fd;             /* ioqueue's max_fd */
        pj_bool_t allow_concur; /* ioqueue's allow_concur setting */
        unsigned epoll_flags;   /* epoll_flags: EXCLUSIVE, ONESHOT */
        int sock_type;          /* tcp or udp? */
        int n_threads;          /* number of worker threads polling ioqueue */

        pj_bool_t cancel_connect; /* TCP connect then cancel immediately */
        pj_bool_t failed_connect; /* TCP connect to nowhere */
        pj_bool_t reject_connect; /* close previously listen() without calling accept */
        unsigned tx_so_buf_size; /* SO_SNDBUF in multples of sizeof(int) */
        unsigned rx_so_buf_size; /* SO_RCVBUF in multples of sizeof(int) */
        unsigned pkt_len;       /* size of each send()/recv() in mult. sizeof(int)  */
        unsigned tx_cnt;        /* total number of ints to send  */
        unsigned rx_cnt;        /* total number of ints to read */
        unsigned n_servers;     /* number of servers (=parallel recv) to instantiate */
        unsigned n_clients;     /* number of clients (=parallel send) to instantiate */
        pj_bool_t sequenced;    /* incoming packets must be in sequence */
        unsigned repeat;
    } cfg;

    struct {
        pj_pool_t *pool;
        pj_ioqueue_t *ioq;
        pj_grp_lock_t *grp_lock;
        pj_sock_t listen_sock;       /* tcp listening socket */
        pj_ioqueue_key_t *listen_key;/* tcp listening key */

        pj_sock_t socks[2];          /* server/client sockets */
        unsigned cnt[2];             /* number of packets sent/received */
        pj_ioqueue_key_t *keys[2];   /* server/client keys */
        pj_status_t connect_status;

        int retcode;                 /* test retcode. non-zero will abort. */

        /* okud: op_key user data */
        unsigned okuds_cnt[2];
        op_key_user_data okuds[2][MAX_ASYNC];

    } state;
};

static pj_ioqueue_callback test_cb;

static op_key_user_data *alloc_okud(test_desc *test, int side)
{
    op_key_user_data *okud;

    assert(test->state.okuds_cnt[side] < MAX_ASYNC);
    okud = &test->state.okuds[side][test->state.okuds_cnt[side]++];
    okud->common.side = side;
    okud->common.status = PJ_EPENDING;
    okud->common.count = 0;
    return okud;
}

static void on_read_complete(pj_ioqueue_key_t *key,
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_read)
{
    test_desc *test = (test_desc*)pj_ioqueue_get_user_data(key);
    op_key_user_data *okud = (op_key_user_data*)op_key->user_data;

    assert(okud->common.side == SERVER);

    do {
        unsigned counter = test->state.cnt[SERVER];

        /* can only handle multiple of ints,*/
        PJ_ASSERT_ON_FAIL(bytes_read<0 || bytes_read%sizeof(int)==0,
                          {
                             test->state.retcode = 410;
                             okud->server.status = PJ_EBUG;
                             return;
                           });

        if (bytes_read != -12345 && bytes_read <= 0 && !IGNORE_ERROR(-bytes_read)) {
            TRACE((THIS_FILE, "op_key:%p: stopping due to read=%ld",
                               op_key, bytes_read));
            PJ_PERROR(1,(THIS_FILE, (pj_status_t)-bytes_read, "%ld is", -bytes_read));
            okud->server.status = (bytes_read == 0)? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
                                  (pj_status_t)-bytes_read;
            break;
        }

        if (bytes_read > 0) {
            pj_lock_acquire((pj_lock_t*)test->state.grp_lock);
            if (test->cfg.sequenced) {
                unsigned *p, *start, *end;
                pj_bool_t has_error = PJ_FALSE;

                start = okud->server.recv_buf;
                end = start + (bytes_read / sizeof(int));

                for (p=start; p!=end; ++p) {
                    counter = test->state.cnt[SERVER]++;
                    if (*p != counter && test->cfg.sock_type==pj_SOCK_STREAM()) {
                        PJ_LOG(3,(THIS_FILE, "  Error: TCP RX sequence mismatch at idx=%ld. Expecting %d, got %d",
                                  p-start, counter, *p));
                        test->state.retcode = 412;
                        okud->server.status = PJ_EBUG;
                        pj_lock_release((pj_lock_t*)test->state.grp_lock);
                        return;
                    } else if (test->cfg.sock_type==pj_SOCK_DGRAM()) {
                        if (*p < counter && !has_error) {
                            /* As it turns out, this could happen sometimes
                             * with UDP even when allow_concurrent is set to FALSE.
                             * Maybe the kernel allows the packet to be out of
                             * order? (tested on Linux epoll). Or could there be
                             * bug somewhere?
                             */
                            PJ_LOG(3,(THIS_FILE, "  UDP RX sequence mismatch at idx=%ld. Expecting %d, got %d",
                                      p-start, counter, *p));
                            //test->state.retcode = 413;
                            //okud->server.status = PJ_EBUG;
                            //pj_lock_release((pj_lock_t*)test->state.grp_lock);
                            //return;
                            has_error = PJ_TRUE;
                        } else if (*p > counter) {
                            test->state.cnt[SERVER] = *p;
                        }
                    }
                }
            } else {
                test->state.cnt[SERVER] += (unsigned)(bytes_read / 4);
            }
            pj_lock_release((pj_lock_t*)test->state.grp_lock);
        }

        if (counter >= test->cfg.rx_cnt) {
            //TRACE((THIS_FILE, "op_key:%p: RX counter reached (%d)",
            //       op_key, counter));
            okud->server.status = PJ_SUCCESS;
            break;
        }

        /* next read */
        bytes_read = test->cfg.pkt_len * sizeof(int);
        okud->server.status = pj_ioqueue_recv(key, op_key, okud->server.recv_buf,
                                              &bytes_read, 0);
        if (okud->server.status != PJ_SUCCESS && okud->server.status != PJ_EPENDING) {
            PJ_PERROR(1,(THIS_FILE, okud->server.status, "pj_ioqueue_recv() error"));
            test->state.retcode = 420;
            return;
        }

    } while (okud->server.status==PJ_SUCCESS &&
             test->state.retcode==0  // <= this can be set by other thread
            );
}

static void on_write_complete(pj_ioqueue_key_t *key,
                              pj_ioqueue_op_key_t *op_key,
                              pj_ssize_t bytes_sent)
{
    test_desc *test = (test_desc*)pj_ioqueue_get_user_data(key);
    op_key_user_data *okud = (op_key_user_data*)op_key->user_data;
    unsigned *p, *end;
    unsigned counter;

    pj_assert(okud != NULL);
    assert(okud->common.side == CLIENT);

    PJ_ASSERT_ON_FAIL(bytes_sent<0 || bytes_sent%sizeof(int)==0,
                      {
                         test->state.retcode = 501;
                         okud->client.status = PJ_EBUG;
                         return;
                       });

    if (bytes_sent != -12345 && bytes_sent <= 0 && !IGNORE_ERROR(-bytes_sent)) {
        TRACE((THIS_FILE, "op_key:%p: stopping due to sent=%ld",
                           op_key, bytes_sent));
        okud->client.status = (bytes_sent == 0)? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
                              (pj_status_t)-bytes_sent;
        return;
    }

    /* for TCP, client stop transmission when number of transmitted
     * count is done. For UDP, continue transmitting indefinitely (until
     * receiver says to stop) because packets may be dropped.
     */
    counter = test->state.cnt[CLIENT];
    if (test->cfg.sock_type==pj_SOCK_STREAM() && counter >= test->cfg.tx_cnt) {
        //TRACE((THIS_FILE, "op_key:%p: TX counter reached (%d)",
        //             op_key, counter));
        okud->client.status = PJ_SUCCESS;
        return;
    }

    /* construct buffer as int sequence.
     * protect with mutex to make sure sending is in sequence.
     */
    pj_lock_acquire((pj_lock_t*)test->state.grp_lock);

    p = okud->client.send_buf;
    end = p + test->cfg.pkt_len;
    while (p != end)
        *p++ = test->state.cnt[CLIENT]++;

    /* next write */
    bytes_sent = test->cfg.pkt_len * sizeof(int);
    okud->client.status = pj_ioqueue_send(key, op_key, okud->client.send_buf,
                                          &bytes_sent, 0);
    if (okud->client.status != PJ_SUCCESS && okud->client.status != PJ_EPENDING) {
        PJ_PERROR(1,(THIS_FILE, okud->client.status, "pj_ioqueue_send() error"));
        test->state.retcode = 520;
    } else if (okud->client.status == PJ_SUCCESS) {
        counter = test->state.cnt[CLIENT];
        if (test->cfg.sock_type==pj_SOCK_STREAM() && counter >= test->cfg.tx_cnt) {
            //TRACE((THIS_FILE, "op_key:%p: TX counter reached (%d)",
            // op_key, counter));
        }
    }

    pj_lock_release((pj_lock_t*)test->state.grp_lock);
}

static void on_accept_complete(pj_ioqueue_key_t *key,
                               pj_ioqueue_op_key_t *op_key,
                               pj_sock_t sock,
                               pj_status_t status)
{
    test_desc *test = (test_desc*)pj_ioqueue_get_user_data(key);
    /* For accept, reuse okud for server socket */
    op_key_user_data *okud = (op_key_user_data*)op_key->user_data;

    assert(okud->common.side == SERVER);
    if (status != PJ_SUCCESS) {
        okud->server.status = status;
        return;
    }

    test->state.socks[SERVER] = sock;
    pj_lock_acquire((pj_lock_t*)test->state.grp_lock);

    if (test->cfg.rx_so_buf_size) {
        int value = test->cfg.rx_so_buf_size * sizeof(int);
        okud->server.status = pj_sock_setsockopt(sock,
                                                 pj_SOL_SOCKET(),
                                                 pj_SO_RCVBUF(),
                                                 &value, sizeof(value));
    }

    status = pj_ioqueue_register_sock2(test->state.pool,
                                       test->state.ioq,
                                       test->state.socks[SERVER],
                                       test->state.grp_lock,
                                       test,
                                       &test_cb,
                                       &test->state.keys[SERVER]);
    if (status != PJ_SUCCESS) {
        pj_lock_release((pj_lock_t*)test->state.grp_lock);
        PJ_PERROR(1,(THIS_FILE, status, "pj_ioqueue_register_sock2 error"));
        test->state.retcode = 605;
        return;
    }

    if (!okud->server.recv_buf) {
        okud->server.recv_buf = pj_pool_calloc(test->state.pool, test->cfg.pkt_len,
                                               sizeof(int));
    }

    pj_ioqueue_op_key_init(&okud->server.recv_op, sizeof(pj_ioqueue_op_key_t));
    okud->server.recv_op.user_data = okud;
    pj_lock_release((pj_lock_t*)test->state.grp_lock);

    on_read_complete(test->state.keys[SERVER], &okud->server.recv_op, -12345);
}

static void on_connect_complete(pj_ioqueue_key_t *key,
                                pj_status_t status)
{
    test_desc *test = (test_desc*)pj_ioqueue_get_user_data(key);
    unsigned i;

#if PJ_WIN64 || PJ_WIN32
    if (test->cfg.expected_ret_code==RETCODE_CONNECT_FAILED && 
        status==PJ_SUCCESS) 
    {
        /* On Windows, when the server socket is closed even without accept(),
        * connect() returns success but subsequent write will return
        * error
        */
        pj_ioqueue_op_key_t op_key;
        int send_buf = 0;
        pj_ssize_t bytes_sent = sizeof(send_buf);

        status = pj_ioqueue_send(key, &op_key, &send_buf, &bytes_sent, 0);
    }
#endif

    test->state.connect_status = status;
    if (status != PJ_SUCCESS) {
        if (test->cfg.expected_ret_code != RETCODE_CONNECT_FAILED)
            PJ_PERROR(1, (THIS_FILE, status, "async connect failed"));
        test->state.retcode = RETCODE_CONNECT_FAILED;
        return;
    }

    for (i=0; i<test->cfg.n_clients; ++i) {
        op_key_user_data *okud;

        pj_lock_acquire((pj_lock_t*)test->state.grp_lock);
        okud = alloc_okud(test, CLIENT);
        if (!okud->client.send_buf) {
            okud->client.send_buf = pj_pool_calloc(test->state.pool,
                                                   test->cfg.pkt_len,
                                                   sizeof(int));
        }

        pj_ioqueue_op_key_init(&okud->client.send_op, sizeof(pj_ioqueue_op_key_t));
        okud->client.send_op.user_data = okud;
        pj_lock_release((pj_lock_t*)test->state.grp_lock);

        on_write_complete(key, &okud->client.send_op, -12345);
    }
};

/* worker thread */
static int worker_thread(void *p)
{
    test_desc *test = (test_desc*)p;
    unsigned n_events = 0;

    /* log indent is not propagated to other threads,
     * so we set it explicitly here
     */
    pj_log_set_indent(3);

    while (test->state.retcode==0) {
        unsigned i;
        pj_time_val timeout = {0, 10};

        int rc = pj_ioqueue_poll(test->state.ioq, &timeout);
        if (rc < 0) {
            PJ_PERROR(1,(THIS_FILE, -rc, "pj_ioqueue_poll() error"));
        }

        if (rc > 0)
            n_events += rc;

        if (test->state.cnt[CLIENT] >= test->cfg.tx_cnt &&
            test->state.cnt[SERVER] >= test->cfg.rx_cnt)
        {
            /* Done. Success */
            break;
        }

        /* Check if client needs to send. UDP client needs to send until
         * server receives enough packet (because packets can be lost).
         * TCP client stops after enough packets are transmitted.
         */
        for (i=0; i<test->state.okuds_cnt[CLIENT] &&
                    (test->cfg.sock_type == pj_SOCK_DGRAM() ||
                     test->state.cnt[CLIENT] < test->cfg.tx_cnt); ++i)
        {
            op_key_user_data *okud = &test->state.okuds[CLIENT][i];
            pj_lock_acquire((pj_lock_t*)test->state.grp_lock);
            if (!pj_ioqueue_is_pending(test->state.keys[CLIENT],
                                       &okud->client.send_op)) {
                on_write_complete(test->state.keys[CLIENT],
                                  &okud->client.send_op, -12345);
            }
            pj_lock_release((pj_lock_t*)test->state.grp_lock);
        }

        if (test->state.connect_status != PJ_SUCCESS &&
            test->state.connect_status != PJ_EPENDING)
        {
            break;
        }
    }

    /* Flush events. Check if polling is blocked (for ioq select, it should,
     * but for epoll and kqueue backends, the blocking duration is just minimal
     * to avoid busy loop).
     */
    if (test->state.retcode==test->cfg.expected_ret_code) {
        pj_timestamp t0, t1;
        unsigned i, msec, duration;

        pj_get_timestamp(&t0);
        for (i=0; i<10; ++i) {
            pj_time_val timeout = {0, 100};
            n_events += pj_ioqueue_poll(test->state.ioq, &timeout);
            pj_thread_sleep(10);
        }
        pj_get_timestamp(&t1);
        msec = pj_elapsed_msec(&t0, &t1);
        duration = (!pj_ansi_strcmp(pj_ioqueue_name(), "select"))? 500: 200;
        if (msec <= duration) {
            test->state.retcode = 5000;
            PJ_LOG(1,(THIS_FILE, "Error: pj_ioqueue_poll is not blocking, "
                                 "time elapsed: %d, no events: %d",
                                 msec, n_events));
        }
    }

    TRACE((THIS_FILE, "thread exiting, n_events=%d", n_events));
    return 0;
}


/* Perform single pass of test loop, reusing the ioqueue/pool state */
static int perform_single_pass(test_desc *test)
{
    unsigned i;
    int retcode = 0;
    pj_sockaddr addr;
    int namelen = sizeof(addr);
    pj_str_t localhost = pj_str("127.0.0.1");

    /* Reset iteration states */
    test->state.retcode = 0;
    test->state.connect_status = PJ_EPENDING;
    test->state.listen_key = NULL;
    test->state.listen_sock = PJ_INVALID_SOCKET;
    for (i=0; i<2; ++i) {
        int j;

        test->state.socks[i] = PJ_INVALID_SOCKET;
        test->state.keys[i] = NULL;
        test->state.cnt[i] = 0;
        test->state.okuds_cnt[i] = 0;

        for (j=0; j<MAX_ASYNC; ++j) {
            //test->state.okuds[i][j].common.status = PJ_EPENDING;
        }
    }

    /*
     * Init server side
     */
    if (test->cfg.sock_type == pj_SOCK_STREAM()) {
        CHECK(20, pj_sock_socket(pj_AF_INET(), test->cfg.sock_type, 0,
                                 &test->state.listen_sock));
        CHECK(21, pj_sock_bind_in(test->state.listen_sock, 0, 0));
        CHECK(22, pj_sock_getsockname(test->state.listen_sock, &addr, &namelen));
        CHECK(23, pj_sock_listen(test->state.listen_sock, 5));
        CHECK(24, pj_ioqueue_register_sock2(test->state.pool,
                                            test->state.ioq,
                                            test->state.listen_sock,
                                            test->state.grp_lock,
                                            test,
                                            &test_cb,
                                            &test->state.listen_key));
        if (!test->cfg.reject_connect) {
            /* kick off parallel accepts */
            for (i=0; i<test->cfg.n_servers; ++i) {
                op_key_user_data *okud;

                okud = alloc_okud(test, SERVER);
                pj_ioqueue_op_key_init(&okud->server.recv_op,
                                       sizeof(okud->server.recv_op));
                okud->server.recv_op.user_data = okud;
                okud->server.status = pj_ioqueue_accept(test->state.listen_key,
                                                        &okud->server.recv_op,
                                                        &okud->server.new_sock,
                                                        NULL,
                                                        NULL,
                                                        NULL);
                CHECK(26, okud->server.status);
            }
        }
    } else {
        CHECK(30, pj_sock_socket(pj_AF_INET(), test->cfg.sock_type, 0,
                                 &test->state.socks[SERVER]));
        CHECK(31, pj_sock_bind_in(test->state.socks[SERVER], 0, 0));
        CHECK(32, pj_sock_getsockname(test->state.socks[SERVER], &addr, &namelen));
        CHECK(33, pj_ioqueue_register_sock2(test->state.pool,
                                            test->state.ioq,
                                            test->state.socks[SERVER],
                                            test->state.grp_lock,
                                            test,
                                            &test_cb,
                                            &test->state.keys[SERVER]));

        if (test->cfg.rx_so_buf_size) {
            int value = test->cfg.rx_so_buf_size * sizeof(int);
            CHECK(34, pj_sock_setsockopt(test->state.socks[SERVER],
                                         pj_SOL_SOCKET(),
                                         pj_SO_RCVBUF(),
                                         &value, sizeof(value)));
        }

        /* kick off parallel recvs */
        for (i=0; i<test->cfg.n_servers; ++i) {
            op_key_user_data *okud;

            okud = alloc_okud(test, SERVER);
            if (!okud->server.recv_buf) {
                okud->server.recv_buf = pj_pool_calloc(test->state.pool,
                                                       test->cfg.pkt_len,
                                                       sizeof(int));
            }
            pj_ioqueue_op_key_init(&okud->server.recv_op, sizeof(pj_ioqueue_op_key_t));
            okud->server.recv_op.user_data = okud;

            on_read_complete(test->state.keys[SERVER], &okud->server.recv_op,
                             -12345);
        }
    }


    /*
     * Client side
     */
    CHECK(40, pj_sock_socket(pj_AF_INET(), test->cfg.sock_type, 0,
                             &test->state.socks[CLIENT]));
    addr.ipv4.sin_addr = pj_inet_addr(&localhost);
    if (test->cfg.tx_so_buf_size) {
        int value = test->cfg.tx_so_buf_size * sizeof(int);
        CHECK(41, pj_sock_setsockopt(test->state.socks[CLIENT],
                                     pj_SOL_SOCKET(),
                                     pj_SO_RCVBUF(),
                                     &value, sizeof(value)));
    }
    CHECK(42, pj_ioqueue_register_sock2(test->state.pool,
                                        test->state.ioq,
                                        test->state.socks[CLIENT],
                                        test->state.grp_lock,
                                        test,
                                        &test_cb,
                                        &test->state.keys[CLIENT]));
    if (test->cfg.sock_type == pj_SOCK_STREAM()) {
        if (test->cfg.failed_connect) {
            pj_sockaddr_in_init((pj_sockaddr_in*)&addr, &localhost, 39275);
            test->state.connect_status = pj_ioqueue_connect(test->state.keys[CLIENT],
                                                            &addr,
                                                            sizeof(pj_sockaddr_in));
            if (test->state.connect_status && test->state.connect_status!=PJ_EPENDING)
                PJ_PERROR(1,(THIS_FILE, test->state.connect_status,
                             "TCP connect() error"));
            CHECK(44, test->state.connect_status);
        } else {
            test->state.connect_status = pj_ioqueue_connect(test->state.keys[CLIENT],
                                                            &addr, namelen);
            CHECK(45, test->state.connect_status);
        }
        /* To be continued when socket is connected (or fails) */
    } else {
        /* UDP connect */
        CHECK(46, pj_ioqueue_connect(test->state.keys[CLIENT],
                                     &addr, namelen));
        /* kick off parallel send */
        for (i=0; i<test->cfg.n_clients; ++i) {
            op_key_user_data *okud;

            okud = alloc_okud(test, CLIENT);
            if (!okud->client.send_buf) {
                okud->client.send_buf = pj_pool_calloc(test->state.pool,
                                                       test->cfg.pkt_len,
                                                       sizeof(int));
            }
            pj_ioqueue_op_key_init(&okud->client.send_op, sizeof(pj_ioqueue_op_key_t));
            okud->client.send_op.user_data = okud;

            on_write_complete(test->state.keys[CLIENT], &okud->client.send_op,
                             -12345);
        }
    }

    if (test->cfg.reject_connect) {
        CHECK(47, pj_ioqueue_unregister(test->state.listen_key));
        test->state.listen_key = NULL;
        test->state.listen_sock = PJ_INVALID_SOCKET;
    } else if (test->cfg.cancel_connect) {
        CHECK(48, pj_ioqueue_unregister(test->state.keys[CLIENT]));
    }

    pj_log_push_indent();

    if (test->cfg.n_threads==0)
        worker_thread(test);
    else {
        unsigned n_threads = test->cfg.n_threads;
        pj_thread_t *threads[MAX_THREADS];

        for (i=0; i<n_threads; ++i) {
            CHECK(49, pj_thread_create(test->state.pool, "ioq_stress_test",
                                       &worker_thread, test,
                                       0, PJ_THREAD_SUSPENDED,
                                       &threads[i]));
        }

        for (i=0; i<n_threads; ++i) {
            pj_status_t status = pj_thread_resume(threads[i]);
            PJ_ASSERT_RETURN(status==PJ_SUCCESS, 49);
        }

        worker_thread(test);

        for (i=0; i<n_threads; ++i) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }
    }

    retcode = test->state.retcode;

on_return:
    if (test->state.listen_key)
        pj_ioqueue_unregister(test->state.listen_key);
    for (i=0; i<2; ++i) {
        if (test->state.keys[i])
            pj_ioqueue_unregister(test->state.keys[i]);
    }

    pj_log_pop_indent();
    return (retcode==test->cfg.expected_ret_code)? 0 : retcode;
}

/* Perform tests according to test_desc */
static int perform_test(test_desc *test)
{
    pj_ioqueue_cfg ioqueue_cfg;
    unsigned rep;
    int retcode;

    PJ_LOG(3,(THIS_FILE, "%s", test->cfg.title));
    pj_log_push_indent();

    PJ_ASSERT_RETURN(test->cfg.n_servers <= MAX_ASYNC, 4000);
    PJ_ASSERT_RETURN(test->cfg.n_clients <= MAX_ASYNC, 4001);

    pj_bzero(&test->state, sizeof(test->state));

    PJ_ASSERT_RETURN(test->state.pool == NULL, -1);
    test->state.pool = pj_pool_create(mem, NULL, 4096, 4096, NULL);

    pj_ioqueue_cfg_default(&ioqueue_cfg);
    ioqueue_cfg.epoll_flags = test->cfg.epoll_flags;

    CHECK(10, pj_ioqueue_create2(test->state.pool, test->cfg.max_fd,
                                 &ioqueue_cfg, &test->state.ioq));
    CHECK(11, pj_ioqueue_set_default_concurrency(test->state.ioq,
                                                 test->cfg.allow_concur));
    CHECK(20, pj_grp_lock_create(test->state.pool, NULL, &test->state.grp_lock));
    CHECK(21, pj_grp_lock_add_ref(test->state.grp_lock));

    for (rep=0, retcode=0; rep<test->cfg.repeat && !retcode; ++rep) {
        TRACE((THIS_FILE, "repeat %d/%d", rep+1, test->cfg.repeat));
        retcode = perform_single_pass(test);
    }

on_return:
    if (test->state.ioq)
        pj_ioqueue_destroy(test->state.ioq);
    if (test->state.grp_lock) {
        pj_grp_lock_dec_ref(test->state.grp_lock);
        test->state.grp_lock = NULL;
    }
    if (test->state.pool)
        pj_pool_release(test->state.pool);

    if (retcode) {
        PJ_LOG(3,(THIS_FILE, "test failed (retcode=%d)", retcode));
    }
    pj_log_pop_indent();
    return retcode;
}


static test_desc tests[128] = {
    /* simplest test, no threads, no parallel send/recv, it's good for debugging.
     * max_fd is limited to test management of closing keys in ioqueue safe
     * unregistration.
     */
    {
        .cfg.title = "basic udp (single thread)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sock_type = SOCK_DGRAM, /* cannot use pj_SOCK_DGRAM (not constant) */
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 512,
        .cfg.rx_cnt = 512,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    #if PJ_HAS_LINUX_EPOLL
    {
        .cfg.title = "basic udp (single thread, EPOLLEXCLUSIVE)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 512,
        .cfg.rx_cnt = 512,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "basic udp (single thread, EPOLLONESHOT)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 512,
        .cfg.rx_cnt = 512,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "basic udp (single thread, epoll plain)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = 0,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 512,
        .cfg.rx_cnt = 512,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    #endif
    /* simplest test (tcp), no threads, no parallel send/recv, it's good for debugging.
     * max_fd is limited to test management of closing keys in ioqueue safe
     * unregistration.
     */
    {
        .cfg.title = "basic tcp (single thread)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    #if PJ_HAS_LINUX_EPOLL
    {
        .cfg.title = "basic tcp (single thread, EPOLLEXCLUSIVE)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "basic tcp (single thread, EPOLLONESHOT)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "basic tcp (single thread, plain)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = 0,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = 0,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 4
    },
    #endif
    /* failed TCP connect().
     */
    {
        .cfg.title = "failed tcp connect (pls wait)",
        .cfg.expected_ret_code = RETCODE_CONNECT_FAILED,
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.failed_connect = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 2
    },
    #if PJ_HAS_LINUX_EPOLL
    {
        .cfg.title = "failed tcp connect (EPOLLEXCLUSIVE)",
        .cfg.expected_ret_code = RETCODE_CONNECT_FAILED,
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.failed_connect = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 2
    },
    {
        .cfg.title = "failed tcp connect (EPOLLONESHOT)",
        .cfg.expected_ret_code = RETCODE_CONNECT_FAILED,
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.failed_connect = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 2
    },
    {
        .cfg.title = "failed tcp connect (plain)",
        .cfg.expected_ret_code = RETCODE_CONNECT_FAILED,
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = 0,
        .cfg.failed_connect = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 2
    },
    #endif
    /* reject TCP connect().
     */
    {
        .cfg.title = "tcp connect rejected",
        .cfg.expected_ret_code = RETCODE_CONNECT_FAILED,
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.reject_connect = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 20,
        .cfg.rx_cnt = 20,
        .cfg.n_servers = 1,
        .cfg.n_clients = 1,
        .cfg.repeat = 2
    },
    /* quite involved test (udp). Multithreads, parallel send/recv operations,
     * limitation in recv buffer. max_fd is very limited to test management of
     * closing keys in ioqueue safe unregistration.
     */
    {
        .cfg.title = "udp (multithreads)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*8*512,
        .cfg.rx_cnt = 4*8*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    #if PJ_HAS_LINUX_EPOLL
    /* EPOLLEXCLUSIVE (udp).
     */
    {
        .cfg.title = "udp (multithreads, EPOLLEXCLUSIVE)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = MAX_THREADS,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*8*512,
        .cfg.rx_cnt = 4*8*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    /* EPOLLONESHOT (udp).
     */
    {
        .cfg.title = "udp (multithreads, EPOLLONESHOT)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = 2,
        .cfg.rx_so_buf_size = 2,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*8*512,
        .cfg.rx_cnt = 4*8*512,
        .cfg.n_servers = 2,
        .cfg.n_clients = 2,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "udp (multithreads, plain epoll)",
        .cfg.max_fd = 4,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = 0,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = 2,
        .cfg.rx_so_buf_size = 2,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*8*512,
        .cfg.rx_cnt = 4*8*512,
        .cfg.n_servers = 2,
        .cfg.n_clients = 2,
        .cfg.repeat = 4
    },
    #endif
    /* quite involved test (tcp). Multithreads, parallel send/recv operations,
     * limitation in recv buffer. max_fd is small/limited to test management of
     * closing keys in ioqueue safe unregistration.
     */
    {
        .cfg.title = "tcp (multithreads)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,        /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    #if PJ_HAS_LINUX_EPOLL
    {
        .cfg.title = "tcp (multithreads, EPOLLEXCLUSIVE)",
        .cfg.max_fd = 6,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.allow_concur = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,        /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "tcp (multithreads, EPOLLONESHOT)",
        .cfg.max_fd = 6,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.allow_concur = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,        /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "tcp (multithreads, plain epoll)",
        .cfg.max_fd = 6,
        .cfg.epoll_flags = 0,
        .cfg.allow_concur = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,        /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    #endif
    /* when concurrency is disabled, TCP packets should be received in correct
     * order
     */
    {
        .cfg.title = "tcp (multithreads, sequenced, concur=0)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 0,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,         /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    #if PJ_HAS_LINUX_EPOLL
    {
        .cfg.title = "tcp (multithreads, sequenced, concur=0, EPOLLEXCLUSIVE)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 0,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_EXCLUSIVE,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,         /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "tcp (multithreads, sequenced, concur=0, EPOLLONESHOT)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 0,
        .cfg.epoll_flags = PJ_IOQUEUE_EPOLL_ONESHOT,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,         /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    {
        .cfg.title = "tcp (multithreads, sequenced, concur=0, epoll plain)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 0,
        .cfg.epoll_flags = 0,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,         /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    #endif
    /* when concurrency is *enabled*, TCP packets are received in correct
     * order (tested with epoll, not sure if this behavior is true for
     * all backends)
     */
    {
        .cfg.title = "tcp (multithreads, sequenced, concur=1)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 1,          /* enabled */
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_STREAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = 2,         /* Set to small to control flow of tcp */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
    /* when concurrency is disabled, UDP packets should be received in increasing
     * order (although some packets may be lost). Edit: it turns out packets
     * can be received out of order, hence sequence verification is disabled
     * in the callback (only warning will be printed).
     */
    {
        .cfg.title = "udp (multithreads, sequenced, concur=0)",
        .cfg.max_fd = 6,
        .cfg.allow_concur = 0,
        .cfg.epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS,
        .cfg.sequenced = 1,
        .cfg.sock_type = SOCK_DGRAM,
        .cfg.n_threads = MAX_THREADS,
        .cfg.rx_so_buf_size = MAX_ASYNC, /* Set to small so some packets will be dropped */
        .cfg.tx_so_buf_size = MAX_ASYNC,
        .cfg.pkt_len = 4,
        .cfg.tx_cnt = 4*MAX_THREADS*512,
        .cfg.rx_cnt = 4*MAX_THREADS*512,
        .cfg.n_servers = MAX_ASYNC,
        .cfg.n_clients = MAX_ASYNC,
        .cfg.repeat = 4
    },
};

int ioqueue_stress_test(void)
{
    int i;
    int retcode = 0;

    pj_log_push_indent();

    test_cb.on_read_complete = on_read_complete;
    test_cb.on_write_complete = on_write_complete;
    test_cb.on_accept_complete = on_accept_complete;
    test_cb.on_connect_complete = on_connect_complete;

    for (i=0; i<(int)PJ_ARRAY_SIZE(tests); ++i) {
        int r;

        test_desc *test = &tests[i];
        if (!test->cfg.title)
            break;

        r = perform_test(&tests[i]);
        if (r && !retcode)
            retcode = r;
    }

    pj_log_pop_indent();
    return retcode;
}


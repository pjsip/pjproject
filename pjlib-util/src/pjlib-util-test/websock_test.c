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

#if INCLUDE_WEBSOCK_TEST

#define THIS_FILE   "websock_test"

#include <pjlib.h>
#include <pjlib-util.h>

/*
 * Minimal WebSocket echo server running on a local thread.
 * Implements just enough of the WebSocket protocol to:
 *   - Accept a handshake
 *   - Echo back text/binary frames
 *   - Respond to close frames
 */

/* ======== Local echo server ======== */

#define SERVER_BUF_SIZE     8192
#define WS_GUID             "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static struct echo_server_t
{
    pj_sock_t       sock;
    pj_uint16_t     port;
    pj_thread_t    *thread;
    pj_bool_t       quit;
} g_server;

/* Minimal Base64 encode (RFC 4648) for the server handshake */
static void b64_encode(const pj_uint8_t *in, int in_len,
                       char *out, int *out_len)
{
    pj_base64_encode(in, in_len, out, out_len);
}

/* Build the server handshake response */
static int build_server_handshake(const char *req, char *resp,
                                  int resp_size)
{
    const char *key_hdr;
    char ws_key[64];
    int key_len;
    char concat[128];
    int concat_len;
    pj_sha1_context ctx;
    pj_uint8_t hash[20];
    char accept_b64[32];
    int accept_len = sizeof(accept_b64);
    const char *key_end;

    key_hdr = strstr(req, "Sec-WebSocket-Key: ");
    if (!key_hdr) return -1;
    key_hdr += 19;
    key_end = strstr(key_hdr, "\r\n");
    if (!key_end) return -1;
    key_len = (int)(key_end - key_hdr);
    if (key_len >= (int)sizeof(ws_key)) return -1;
    pj_memcpy(ws_key, key_hdr, key_len);
    ws_key[key_len] = '\0';

    concat_len = pj_ansi_snprintf(concat, sizeof(concat), "%s%s",
                                  ws_key, WS_GUID);
    pj_sha1_init(&ctx);
    pj_sha1_update(&ctx, (pj_uint8_t*)concat, concat_len);
    pj_sha1_final(&ctx, hash);
    b64_encode(hash, 20, accept_b64, &accept_len);
    accept_b64[accept_len] = '\0';

    return pj_ansi_snprintf(resp, resp_size,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_b64);
}

/* Decode a client WebSocket frame (masked).
 * Returns total consumed bytes, 0 if incomplete.
 */
static pj_size_t server_decode_frame(const pj_uint8_t *data, pj_size_t len,
                                     pj_uint8_t *opcode, pj_bool_t *fin,
                                     pj_uint8_t *payload, pj_size_t *plen)
{
    pj_size_t pos = 0;
    pj_size_t pl;
    pj_uint8_t mask[4];
    pj_size_t i;

    if (len < 2) return 0;
    *fin = (data[0] & 0x80) != 0;
    *opcode = data[0] & 0x0F;
    pl = data[1] & 0x7F;
    pos = 2;
    if (pl == 126) {
        if (len < 4) return 0;
        pl = ((pj_size_t)data[2] << 8) | data[3];
        pos = 4;
    } else if (pl == 127) {
        if (len < 10) return 0;
        pl = ((pj_size_t)data[6] << 24) | ((pj_size_t)data[7] << 16) |
             ((pj_size_t)data[8] << 8) | data[9];
        pos = 10;
    }
    /* Client frames must be masked */
    if (!(data[1] & 0x80)) return 0;
    if (len < pos + 4 + pl) return 0;
    pj_memcpy(mask, &data[pos], 4);
    pos += 4;
    for (i = 0; i < pl; ++i)
        payload[i] = data[pos + i] ^ mask[i & 3];
    *plen = pl;
    return pos + pl;
}

/* Encode a server WebSocket frame (unmasked) */
static pj_size_t server_encode_frame(pj_uint8_t *buf, pj_uint8_t opcode,
                                     const pj_uint8_t *payload,
                                     pj_size_t plen)
{
    pj_size_t pos = 0;
    buf[pos++] = (pj_uint8_t)(0x80 | opcode); /* FIN + opcode */
    if (plen < 126) {
        buf[pos++] = (pj_uint8_t)plen;
    } else if (plen <= 0xFFFF) {
        buf[pos++] = 126;
        buf[pos++] = (pj_uint8_t)((plen >> 8) & 0xFF);
        buf[pos++] = (pj_uint8_t)(plen & 0xFF);
    } else {
        buf[pos++] = 127;
        buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
        buf[pos++] = (pj_uint8_t)((plen >> 24) & 0xFF);
        buf[pos++] = (pj_uint8_t)((plen >> 16) & 0xFF);
        buf[pos++] = (pj_uint8_t)((plen >> 8) & 0xFF);
        buf[pos++] = (pj_uint8_t)(plen & 0xFF);
    }
    if (plen > 0)
        pj_memcpy(&buf[pos], payload, plen);
    return pos + plen;
}

static int server_thread(void *p)
{
    struct echo_server_t *srv = (struct echo_server_t*)p;
    pj_uint8_t buf[SERVER_BUF_SIZE];
    pj_uint8_t frame_payload[SERVER_BUF_SIZE];
    pj_uint8_t out_buf[SERVER_BUF_SIZE];

    while (!srv->quit) {
        pj_sock_t csock = PJ_INVALID_SOCKET;
        pj_ssize_t rlen;
        int rc;
        pj_fd_set_t rset;
        pj_time_val timeout;
        pj_size_t total;
        pj_bool_t handshake_done;

        /* Accept with select timeout */
        timeout.sec = 0;
        timeout.msec = 200;
        PJ_FD_ZERO(&rset);
        PJ_FD_SET(srv->sock, &rset);
        rc = pj_sock_select((int)srv->sock + 1, &rset, NULL, NULL,
                            &timeout);
        if (rc != 1 || srv->quit)
            continue;

        if (pj_sock_accept(srv->sock, &csock, NULL, NULL) != PJ_SUCCESS)
            continue;

        /* Handle one client connection */
        total = 0;
        handshake_done = PJ_FALSE;

        while (!srv->quit) {
            timeout.sec = 5;
            timeout.msec = 0;
            PJ_FD_ZERO(&rset);
            PJ_FD_SET(csock, &rset);
            rc = pj_sock_select((int)csock + 1, &rset, NULL, NULL,
                                &timeout);
            if (rc != 1)
                break;

            rlen = sizeof(buf) - total;
            if (pj_sock_recv(csock, buf + total, &rlen, 0) != PJ_SUCCESS
                || rlen <= 0)
            {
                break;
            }
            total += rlen;

            if (!handshake_done) {
                char resp[512];
                int resp_len;
                pj_ssize_t slen;

                /* Wait for full HTTP request */
                if (!strstr((char*)buf, "\r\n\r\n"))
                    continue;

                resp_len = build_server_handshake((char*)buf, resp,
                                                  sizeof(resp));
                if (resp_len < 0) break;
                slen = resp_len;
                pj_sock_send(csock, resp, &slen, 0);
                handshake_done = PJ_TRUE;
                total = 0;
                continue;
            }

            /* Process WebSocket frames */
            while (total > 0) {
                pj_uint8_t opcode;
                pj_bool_t fin;
                pj_size_t plen = 0;
                pj_size_t consumed;
                pj_size_t out_len;
                pj_ssize_t slen;

                consumed = server_decode_frame(buf, total, &opcode, &fin,
                                               frame_payload, &plen);
                if (consumed == 0)
                    break; /* incomplete frame */

                if (opcode == 0x8) {
                    /* Close frame: echo it back and disconnect */
                    out_len = server_encode_frame(out_buf, 0x8,
                                                  frame_payload, plen);
                    slen = (pj_ssize_t)out_len;
                    pj_sock_send(csock, out_buf, &slen, 0);
                    goto client_done;
                } else if (opcode == 0x9) {
                    /* Ping: respond with pong */
                    out_len = server_encode_frame(out_buf, 0xA,
                                                  frame_payload, plen);
                    slen = (pj_ssize_t)out_len;
                    pj_sock_send(csock, out_buf, &slen, 0);
                } else {
                    /* Text/Binary: echo back */
                    out_len = server_encode_frame(out_buf, opcode,
                                                  frame_payload, plen);
                    slen = (pj_ssize_t)out_len;
                    pj_sock_send(csock, out_buf, &slen, 0);
                }

                if (consumed < total)
                    pj_memmove(buf, buf + consumed, total - consumed);
                total -= consumed;
            }
        }

client_done:
        if (csock != PJ_INVALID_SOCKET)
            pj_sock_close(csock);
    }

    return 0;
}

static pj_status_t start_echo_server(void)
{
    pj_sockaddr_in addr;
    pj_status_t status;
    int addr_len;

    g_server.quit = PJ_FALSE;

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0,
                            &g_server.sock);
    if (status != PJ_SUCCESS) return status;

    pj_sockaddr_in_init(&addr, NULL, 0); /* bind to any port */
    status = pj_sock_bind(g_server.sock, &addr, sizeof(addr));
    if (status != PJ_SUCCESS) {
        pj_sock_close(g_server.sock);
        return status;
    }

    /* Get assigned port */
    addr_len = sizeof(addr);
    pj_sock_getsockname(g_server.sock, &addr, &addr_len);
    g_server.port = pj_sockaddr_in_get_port(&addr);

    status = pj_sock_listen(g_server.sock, 1);
    if (status != PJ_SUCCESS) {
        pj_sock_close(g_server.sock);
        return status;
    }

    status = pj_thread_create(pj_pool_create(mem, "srv", 512, 256, NULL),
                              "ws_echo_srv", &server_thread, &g_server,
                              0, 0, &g_server.thread);
    if (status != PJ_SUCCESS) {
        pj_sock_close(g_server.sock);
        return status;
    }

    PJ_LOG(3, (THIS_FILE, "Echo server started on port %d",
               g_server.port));
    return PJ_SUCCESS;
}

static void stop_echo_server(void)
{
    g_server.quit = PJ_TRUE;
    if (g_server.thread) {
        pj_thread_join(g_server.thread);
        pj_thread_destroy(g_server.thread);
        g_server.thread = NULL;
    }
    if (g_server.sock != PJ_INVALID_SOCKET) {
        pj_sock_close(g_server.sock);
        g_server.sock = PJ_INVALID_SOCKET;
    }
}


/* ======== Client test state ======== */

#define MAX_RX_MSGS        16
#define MAX_MSG_SIZE        4096

typedef struct test_state_t
{
    pj_pool_t              *pool;
    pj_ioqueue_t           *ioq;
    pj_timer_heap_t        *timer;
    pj_websock             *ws;

    /* Results */
    pj_status_t             connect_status;
    pj_bool_t               connected;
    pj_bool_t               closed;
    pj_uint16_t             close_code;

    unsigned                rx_count;
    pj_websock_opcode       rx_opcode[MAX_RX_MSGS];
    pj_size_t               rx_len[MAX_RX_MSGS];
    pj_uint8_t              rx_data[MAX_RX_MSGS][MAX_MSG_SIZE];

    pj_bool_t               done;
} test_state_t;

/* Poll ioqueue and timer until timeout, done, or (optionally) connected */
static void poll_events_ex(test_state_t *st, unsigned timeout_ms,
                           pj_bool_t break_on_connect)
{
    pj_time_val end, now, poll_delay;

    pj_gettimeofday(&end);
    end.msec += timeout_ms;
    pj_time_val_normalize(&end);

    for (;;) {
        poll_delay.sec = 0;
        poll_delay.msec = 10;
        pj_timer_heap_poll(st->timer, NULL);
        pj_ioqueue_poll(st->ioq, &poll_delay);
        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, end) || st->done)
            break;
        if (break_on_connect && st->connected)
            break;
    }
}

/* Poll ioqueue and timer */
static void poll_events(test_state_t *st, unsigned timeout_ms)
{
    poll_events_ex(st, timeout_ms, PJ_FALSE);
}

/* ======== Callbacks ======== */

static void on_connect(pj_websock *ws, pj_status_t status)
{
    test_state_t *st = (test_state_t*)pj_websock_get_user_data(ws);
    st->connect_status = status;
    st->connected = (status == PJ_SUCCESS);
    PJ_LOG(4, (THIS_FILE, "on_connect: status=%d", status));
}

static void on_rx_msg(pj_websock *ws, pj_websock_opcode opcode,
                      const void *data, pj_size_t len)
{
    test_state_t *st = (test_state_t*)pj_websock_get_user_data(ws);
    PJ_LOG(4, (THIS_FILE, "on_rx_msg: opcode=%d len=%lu",
               opcode, (unsigned long)len));
    if (st->rx_count < MAX_RX_MSGS) {
        unsigned idx = st->rx_count;
        st->rx_opcode[idx] = opcode;
        st->rx_len[idx] = len;
        if (len > MAX_MSG_SIZE) len = MAX_MSG_SIZE;
        pj_memcpy(st->rx_data[idx], data, len);
        st->rx_count++;
    }
}

static void on_close(pj_websock *ws, pj_uint16_t status_code,
                     const pj_str_t *reason)
{
    test_state_t *st = (test_state_t*)pj_websock_get_user_data(ws);
    PJ_UNUSED_ARG(reason);
    st->closed = PJ_TRUE;
    st->close_code = status_code;
    st->done = PJ_TRUE;
    PJ_LOG(4, (THIS_FILE, "on_close: code=%d", status_code));
}


/* ======== Test: Create / Destroy ======== */
static int test_create_destroy(void)
{
    pj_pool_t *pool;
    pj_ioqueue_t *ioq;
    pj_timer_heap_t *timer;
    pj_websock *ws = NULL;
    pj_websock_param param;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_create_destroy"));

    pool = pj_pool_create(mem, "wscrt", 4096, 1024, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -100);

    PJ_TEST_SUCCESS(pj_ioqueue_create(pool, 4, &ioq), NULL, {
        pj_pool_release(pool); return -110;
    });
    PJ_TEST_SUCCESS(pj_timer_heap_create(pool, 4, &timer), NULL, {
        pj_ioqueue_destroy(ioq); pj_pool_release(pool); return -120;
    });

    pj_websock_param_default(&param);
    PJ_TEST_EQ(param.ping_interval, PJ_WEBSOCK_DEFAULT_PING_INTERVAL,
               NULL, return -130);

    param.ioqueue = ioq;
    param.timer_heap = timer;

    status = pj_websock_create(pool, &param, &ws);
    PJ_TEST_SUCCESS(status, NULL, {
        pj_ioqueue_destroy(ioq); pj_pool_release(pool); return -140;
    });
    PJ_TEST_NOT_NULL(ws, NULL, return -150);
    PJ_TEST_EQ(pj_websock_get_state(ws), PJ_WEBSOCK_STATE_CLOSED,
               NULL, return -160);

    pj_websock_destroy(ws);
    pj_ioqueue_destroy(ioq);
    pj_timer_heap_destroy(timer);
    pj_pool_release(pool);

    return 0;
}


/* ======== Helper: setup client test state ======== */
static pj_status_t setup_test_state(test_state_t *st)
{
    pj_websock_param param;
    pj_status_t status;

    pj_bzero(st, sizeof(*st));
    st->pool = pj_pool_create(mem, "wstest", 8192, 4096, NULL);
    if (!st->pool) return PJ_ENOMEM;

    status = pj_ioqueue_create(st->pool, 8, &st->ioq);
    if (status != PJ_SUCCESS) return status;

    status = pj_timer_heap_create(st->pool, 8, &st->timer);
    if (status != PJ_SUCCESS) return status;

    pj_websock_param_default(&param);
    param.ioqueue = st->ioq;
    param.timer_heap = st->timer;
    param.ping_interval = 0; /* disable for tests */
    param.cb.on_connect = &on_connect;
    param.cb.on_rx_msg = &on_rx_msg;
    param.cb.on_close = &on_close;
    param.user_data = st;

    return pj_websock_create(st->pool, &param, &st->ws);
}

static void teardown_test_state(test_state_t *st)
{
    if (st->ws)
        pj_websock_destroy(st->ws);
    if (st->ioq)
        pj_ioqueue_destroy(st->ioq);
    if (st->timer)
        pj_timer_heap_destroy(st->timer);
    if (st->pool)
        pj_pool_release(st->pool);
}


/* ======== Test: Text echo (simulates JSON control messages) ======== */
static int test_text_echo(void)
{
    test_state_t st;
    pj_str_t url;
    char url_buf[64];
    const char *json_msg = "{\"type\":\"session.create\","
                           "\"model\":\"gpt-4o-realtime\"}";
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_text_echo (JSON control message)"));

    PJ_TEST_SUCCESS(setup_test_state(&st), NULL, return -200);

    pj_ansi_snprintf(url_buf, sizeof(url_buf),
                     "ws://127.0.0.1:%d/v1/realtime", g_server.port);
    pj_cstr(&url, url_buf);

    status = pj_websock_connect(st.ws, &url, NULL);
    PJ_TEST_TRUE(status == PJ_SUCCESS || status == PJ_EPENDING, NULL, {
        teardown_test_state(&st); return -210;
    });

    /* Wait for connection */
    poll_events_ex(&st, 2000, PJ_TRUE);
    PJ_TEST_TRUE(st.connected, "WebSocket connect failed", {
        teardown_test_state(&st); return -220;
    });

    /* Send JSON text message */
    status = pj_websock_send(st.ws, PJ_WEBSOCK_OP_TEXT,
                             json_msg, pj_ansi_strlen(json_msg));
    PJ_TEST_SUCCESS(status, NULL, {
        teardown_test_state(&st); return -230;
    });

    /* Wait for echo */
    poll_events(&st, 1000);

    PJ_TEST_GTE(st.rx_count, 1U, "No message received", {
        teardown_test_state(&st); return -240;
    });
    PJ_TEST_EQ(st.rx_opcode[0], PJ_WEBSOCK_OP_TEXT,
               "Expected text frame", {
        teardown_test_state(&st); return -250;
    });
    PJ_TEST_EQ(st.rx_len[0], pj_ansi_strlen(json_msg),
               "Echo length mismatch", {
        teardown_test_state(&st); return -260;
    });
    PJ_TEST_EQ(pj_memcmp(st.rx_data[0], json_msg, st.rx_len[0]), 0,
               "Echo content mismatch", {
        teardown_test_state(&st); return -270;
    });

    /* Close */
    pj_websock_close(st.ws, 1000, NULL);
    poll_events(&st, 1000);

    teardown_test_state(&st);
    return 0;
}


/* ======== Test: Binary echo (simulates PCM audio frames) ======== */
static int test_binary_echo(void)
{
    test_state_t st;
    pj_str_t url;
    char url_buf[64];
    pj_uint8_t pcm_buf[960];  /* 20ms @ 24kHz mono 16-bit */
    unsigned i;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_binary_echo (PCM audio frame)"));

    PJ_TEST_SUCCESS(setup_test_state(&st), NULL, return -300);

    pj_ansi_snprintf(url_buf, sizeof(url_buf),
                     "ws://127.0.0.1:%d/audio", g_server.port);
    pj_cstr(&url, url_buf);

    status = pj_websock_connect(st.ws, &url, NULL);
    PJ_TEST_TRUE(status == PJ_SUCCESS || status == PJ_EPENDING, NULL, {
        teardown_test_state(&st); return -310;
    });

    poll_events_ex(&st, 2000, PJ_TRUE);
    PJ_TEST_TRUE(st.connected, "WebSocket connect failed", {
        teardown_test_state(&st); return -320;
    });

    /* Fill with synthetic audio (sine-like pattern) */
    for (i = 0; i < sizeof(pcm_buf); ++i) {
        pcm_buf[i] = (pj_uint8_t)(i & 0xFF);
    }

    /* Send binary audio frame */
    status = pj_websock_send(st.ws, PJ_WEBSOCK_OP_BIN,
                             pcm_buf, sizeof(pcm_buf));
    PJ_TEST_SUCCESS(status, NULL, {
        teardown_test_state(&st); return -330;
    });

    poll_events(&st, 1000);

    PJ_TEST_GTE(st.rx_count, 1U, "No audio echo received", {
        teardown_test_state(&st); return -340;
    });
    PJ_TEST_EQ(st.rx_opcode[0], PJ_WEBSOCK_OP_BIN,
               "Expected binary frame", {
        teardown_test_state(&st); return -350;
    });
    PJ_TEST_EQ(st.rx_len[0], sizeof(pcm_buf),
               "Audio echo length mismatch", {
        teardown_test_state(&st); return -360;
    });
    PJ_TEST_EQ(pj_memcmp(st.rx_data[0], pcm_buf, sizeof(pcm_buf)), 0,
               "Audio echo content mismatch", {
        teardown_test_state(&st); return -370;
    });

    pj_websock_close(st.ws, 1000, NULL);
    poll_events(&st, 1000);

    teardown_test_state(&st);
    return 0;
}


/* ======== Test: Multiple rapid sends (simulates streaming audio) ======== */
static int test_streaming(void)
{
    test_state_t st;
    pj_str_t url;
    char url_buf[64];
    pj_uint8_t pcm_buf[480]; /* 10ms @ 24kHz mono 16-bit */
    unsigned i;
    unsigned num_frames = 5;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_streaming (%d audio frames)", num_frames));

    PJ_TEST_SUCCESS(setup_test_state(&st), NULL, return -400);

    pj_ansi_snprintf(url_buf, sizeof(url_buf),
                     "ws://127.0.0.1:%d/stream", g_server.port);
    pj_cstr(&url, url_buf);

    status = pj_websock_connect(st.ws, &url, NULL);
    PJ_TEST_TRUE(status == PJ_SUCCESS || status == PJ_EPENDING, NULL, {
        teardown_test_state(&st); return -410;
    });

    poll_events_ex(&st, 2000, PJ_TRUE);
    PJ_TEST_TRUE(st.connected, NULL, {
        teardown_test_state(&st); return -420;
    });

    /* Send multiple frames with brief polls between each to allow
     * send completion (simulates real 20ms audio frame intervals). */
    for (i = 0; i < num_frames; ++i) {
        pj_memset(pcm_buf, (pj_uint8_t)i, sizeof(pcm_buf));
        status = pj_websock_send(st.ws, PJ_WEBSOCK_OP_BIN,
                                 pcm_buf, sizeof(pcm_buf));
        PJ_TEST_SUCCESS(status, "Send failed mid-stream", {
            teardown_test_state(&st); return -430;
        });
        poll_events(&st, 20);
    }

    /* Wait for all echoes */
    poll_events(&st, 3000);

    PJ_TEST_EQ(st.rx_count, num_frames, "Not all frames echoed", {
        teardown_test_state(&st); return -440;
    });

    /* Verify each echo */
    for (i = 0; i < num_frames; ++i) {
        PJ_TEST_EQ(st.rx_opcode[i], PJ_WEBSOCK_OP_BIN, NULL, {
            teardown_test_state(&st); return -450;
        });
        PJ_TEST_EQ(st.rx_len[i], sizeof(pcm_buf), NULL, {
            teardown_test_state(&st); return -460;
        });
        /* First byte of each frame should match frame index */
        PJ_TEST_EQ(st.rx_data[i][0], (pj_uint8_t)i, NULL, {
            teardown_test_state(&st); return -470;
        });
    }

    pj_websock_close(st.ws, 1000, NULL);
    poll_events(&st, 1000);

    teardown_test_state(&st);
    return 0;
}


/* ======== Test: Graceful close ======== */
static int test_close(void)
{
    test_state_t st;
    pj_str_t url;
    char url_buf[64];
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_close (graceful close handshake)"));

    PJ_TEST_SUCCESS(setup_test_state(&st), NULL, return -500);

    pj_ansi_snprintf(url_buf, sizeof(url_buf),
                     "ws://127.0.0.1:%d/close", g_server.port);
    pj_cstr(&url, url_buf);

    status = pj_websock_connect(st.ws, &url, NULL);
    PJ_TEST_TRUE(status == PJ_SUCCESS || status == PJ_EPENDING, NULL, {
        teardown_test_state(&st); return -510;
    });

    poll_events_ex(&st, 2000, PJ_TRUE);
    PJ_TEST_TRUE(st.connected, NULL, {
        teardown_test_state(&st); return -520;
    });

    /* Initiate close */
    status = pj_websock_close(st.ws, 1000, NULL);
    PJ_TEST_SUCCESS(status, NULL, {
        teardown_test_state(&st); return -530;
    });

    poll_events(&st, 2000);

    PJ_TEST_TRUE(st.closed, "Close callback not received", {
        teardown_test_state(&st); return -540;
    });
    PJ_TEST_EQ(st.close_code, 1000, "Expected close code 1000", {
        teardown_test_state(&st); return -550;
    });
    PJ_TEST_EQ(pj_websock_get_state(st.ws), PJ_WEBSOCK_STATE_CLOSED,
               NULL, {
        teardown_test_state(&st); return -560;
    });

    teardown_test_state(&st);
    return 0;
}


/* ======== Online tests (disabled by default) ======== */

#if INCLUDE_WEBSOCK_ONLINE_TEST

#define PUBLIC_WSS_URL  "wss://echo.websocket.org"

/**
 * Test: Public wss:// echo server.
 * Validates TLS WebSocket handshake and text+binary echo against a real
 * public server. All real-time speech AI APIs (OpenAI Realtime, Gemini Live,
 * etc.) require wss://, so this is the relevant online test.
 */
static int test_public_wss_echo(void)
{
#if PJ_HAS_SSL_SOCK
    test_state_t st;
    pj_str_t url;
    const char *json_msg = "{\"type\":\"test\",\"msg\":\"hello\"}";
    pj_uint8_t bin_buf[320]; /* 10ms @ 16kHz mono 16-bit */
    unsigned i, base_rx;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  test_public_wss_echo (%s)", PUBLIC_WSS_URL));

    PJ_TEST_SUCCESS(setup_test_state(&st), NULL, return -800);

    pj_cstr(&url, PUBLIC_WSS_URL);
    status = pj_websock_connect(st.ws, &url, NULL);
    PJ_TEST_TRUE(status == PJ_SUCCESS || status == PJ_EPENDING, NULL, {
        teardown_test_state(&st); return -810;
    });

    /* Wait for TLS connection (can be slower) */
    poll_events_ex(&st, 15000, PJ_TRUE);
    PJ_TEST_TRUE(st.connected, "Public wss:// connect failed", {
        teardown_test_state(&st); return -820;
    });

    /* Server may send a greeting message first, consume it */
    poll_events(&st, 2000);
    base_rx = st.rx_count;

    /* Send JSON text message (simulates AI session control) */
    status = pj_websock_send(st.ws, PJ_WEBSOCK_OP_TEXT,
                             json_msg, pj_ansi_strlen(json_msg));
    PJ_TEST_SUCCESS(status, NULL, {
        teardown_test_state(&st); return -830;
    });

    poll_events(&st, 5000);
    PJ_TEST_GTE(st.rx_count, base_rx + 1,
                "No text echo from wss:// server", {
        teardown_test_state(&st); return -840;
    });
    PJ_TEST_EQ(st.rx_opcode[base_rx], PJ_WEBSOCK_OP_TEXT, NULL, {
        teardown_test_state(&st); return -850;
    });
    PJ_TEST_EQ(st.rx_len[base_rx], pj_ansi_strlen(json_msg),
               "WSS text echo length mismatch", {
        teardown_test_state(&st); return -860;
    });
    PJ_TEST_EQ(pj_memcmp(st.rx_data[base_rx], json_msg,
                          st.rx_len[base_rx]), 0,
               "WSS text echo content mismatch", {
        teardown_test_state(&st); return -870;
    });

    /* Send binary frame (simulates PCM audio) */
    for (i = 0; i < sizeof(bin_buf); ++i)
        bin_buf[i] = (pj_uint8_t)(i & 0xFF);

    status = pj_websock_send(st.ws, PJ_WEBSOCK_OP_BIN,
                             bin_buf, sizeof(bin_buf));
    PJ_TEST_SUCCESS(status, NULL, {
        teardown_test_state(&st); return -875;
    });

    poll_events(&st, 5000);
    PJ_TEST_GTE(st.rx_count, base_rx + 2,
                "No binary echo from wss:// server", {
        teardown_test_state(&st); return -876;
    });
    PJ_TEST_EQ(st.rx_opcode[base_rx + 1], PJ_WEBSOCK_OP_BIN, NULL, {
        teardown_test_state(&st); return -877;
    });
    PJ_TEST_EQ(st.rx_len[base_rx + 1], sizeof(bin_buf),
               "WSS binary echo length mismatch", {
        teardown_test_state(&st); return -878;
    });
    PJ_TEST_EQ(pj_memcmp(st.rx_data[base_rx + 1], bin_buf,
                          sizeof(bin_buf)), 0,
               "WSS binary echo content mismatch", {
        teardown_test_state(&st); return -879;
    });

    /* Graceful close */
    pj_websock_close(st.ws, 1000, NULL);
    poll_events(&st, 5000);

    PJ_TEST_TRUE(st.closed, "WSS close not received", {
        teardown_test_state(&st); return -880;
    });

    teardown_test_state(&st);
    return 0;

#else  /* PJ_HAS_SSL_SOCK */
    PJ_LOG(3, (THIS_FILE, "  test_public_wss_echo SKIPPED "
                           "(SSL not available)"));
    return 0;
#endif
}

#endif /* INCLUDE_WEBSOCK_ONLINE_TEST */


/* ======== Main entry ======== */
int websock_test(void)
{
    int rc;

    PJ_LOG(3, (THIS_FILE, "WebSocket test (real-time speech AI)"));

    PJ_TEST_SUCCESS(start_echo_server(), "Failed to start echo server",
                    return -10);

    rc = test_create_destroy();
    if (rc) goto on_return;

    rc = test_text_echo();
    if (rc) goto on_return;

    rc = test_binary_echo();
    if (rc) goto on_return;

    rc = test_streaming();
    if (rc) goto on_return;

    rc = test_close();
    if (rc) goto on_return;

on_return:
    stop_echo_server();

#if INCLUDE_WEBSOCK_ONLINE_TEST
    if (rc == 0) {
        PJ_LOG(3, (THIS_FILE, "Online WebSocket tests"));
        rc = test_public_wss_echo();
    }
#endif

    return rc;
}

#endif /* INCLUDE_WEBSOCK_TEST */

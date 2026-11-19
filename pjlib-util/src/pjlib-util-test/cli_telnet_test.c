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

#if INCLUDE_CLI_TELNET_TEST

#define THIS_FILE   "cli_telnet_test"

#include <pjlib.h>
#include <pjlib-util.h>

/* Log text with a CR two bytes before the LF, followed by a CR */
static const char crlf_log[] = "A\rB\n\rafter\n";

static pj_cli_t *cli;
static volatile pj_bool_t log_done;

/* Read from the telnet client until marker is received */
static pj_bool_t recv_marker(pj_sock_t sock, char *buf, int buf_size,
                             int *buf_len, const char *marker,
                             unsigned timeout_ms)
{
    pj_time_val end, now;

    pj_gettickcount(&end);
    end.msec += timeout_ms;
    pj_time_val_normalize(&end);

    for (;;) {
        pj_fd_set_t rset;
        pj_time_val timeout = {0, 50};
        pj_ssize_t len;

        buf[*buf_len] = '\0';
        if (pj_ansi_strstr(buf, marker))
            return PJ_TRUE;

        pj_gettickcount(&now);
        if (PJ_TIME_VAL_GTE(now, end) || *buf_len >= buf_size - 1)
            return PJ_FALSE;

        PJ_FD_ZERO(&rset);
        PJ_FD_SET(sock, &rset);
        if (pj_sock_select((int)sock + 1, &rset, NULL, NULL, &timeout) <= 0)
            continue;

        len = buf_size - 1 - *buf_len;
        if (pj_sock_recv(sock, buf + *buf_len, &len, 0) != PJ_SUCCESS ||
            len <= 0)
        {
            return PJ_FALSE;
        }

        /* Telnet commands may contain NUL */
        while (len--) {
            if (buf[*buf_len] == '\0')
                buf[*buf_len] = ' ';
            ++(*buf_len);
        }
    }
}

static int log_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);

    pj_cli_write_log(cli, 3, crlf_log, (int)sizeof(crlf_log) - 1);
    log_done = PJ_TRUE;
    return 0;
}

int cli_telnet_test(void)
{
    pj_cli_cfg cli_cfg;
    pj_cli_telnet_cfg tel_cfg;
    pj_cli_front_end *fe;
    pj_cli_telnet_info info;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_sockaddr_in addr;
    pj_str_t localhost = {"127.0.0.1", 9};
    pj_pool_t *pool = NULL;
    pj_thread_t *thread = NULL;
    char buf[1024];
    int buf_len = 0;
    unsigned i;
    int rc = 0;

    pj_cli_cfg_default(&cli_cfg);
    cli_cfg.name = pj_str("cli_telnet_test");
    cli_cfg.pf = mem;
    if (pj_cli_create(&cli_cfg, &cli) != PJ_SUCCESS)
        return -10;

    pj_cli_telnet_cfg_default(&tel_cfg);
    tel_cfg.port = 0;
    if (pj_cli_telnet_create(cli, &tel_cfg, &fe) != PJ_SUCCESS) {
        rc = -20;
        goto on_return;
    }
    if (pj_cli_telnet_get_info(fe, &info) != PJ_SUCCESS) {
        rc = -25;
        goto on_return;
    }

    if (pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock) !=
        PJ_SUCCESS)
    {
        rc = -30;
        goto on_return;
    }
    pj_sockaddr_in_init(&addr, &localhost, info.port);
    if (pj_sock_connect(sock, &addr, sizeof(addr)) != PJ_SUCCESS) {
        rc = -40;
        goto on_return;
    }

    /* The session is registered asynchronously, log until it gets one */
    for (i = 0; i < 50; ++i) {
        pj_cli_write_log(cli, 3, "probe\n", 6);
        if (recv_marker(sock, buf, sizeof(buf), &buf_len, "probe", 100))
            break;
    }
    if (i == 50) {
        rc = -50;
        goto on_return;
    }

    pool = pj_pool_create(mem, "cli_telnet_test", 512, 512, NULL);
    log_done = PJ_FALSE;
    if (pj_thread_create(pool, "cli_log", &log_thread, NULL, 0, 0,
                         &thread) != PJ_SUCCESS)
    {
        rc = -60;
        goto on_return;
    }

    for (i = 0; i < 300 && !log_done; ++i)
        pj_thread_sleep(10);
    if (!log_done) {
        /* The log writer is stuck holding the front end mutex, so leave
         * the cli, the thread and its pool alone.
         */
        PJ_LOG(1, (THIS_FILE, "pj_cli_write_log() did not return"));
        rc = -70;
        goto on_return;
    }

    if (!recv_marker(sock, buf, sizeof(buf), &buf_len, "after\r\n", 3000))
        rc = -80;

on_return:
    if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);
    if (rc == -70)
        return rc;
    if (thread) {
        pj_thread_join(thread);
        pj_thread_destroy(thread);
    }
    pj_cli_destroy(cli);
    if (pool)
        pj_pool_release(pool);
    return rc;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled.
 */
int dummy_cli_telnet_test;
#endif  /* INCLUDE_CLI_TELNET_TEST */

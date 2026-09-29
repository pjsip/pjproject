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

/*
 * Failed DNS SRV targets (RFC 3263 section 4.3).
 *
 * The resolver cache holds three SRV targets with priorities 1, 2 and 3,
 * all on 127.0.0.1 with different ports. Each target is a small SIP
 * server whose behaviour is set per case, and the test checks which of
 * them a request sent with pjsip_endpt_send_request() reaches, and the
 * final status the application gets.
 */

#include "test.h"
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>

#define THIS_FILE   "srv_failover_test.c"

#if PJSIP_HAS_RESOLVER && PJ_HAS_THREADS

#define TEST_DOMAIN     "failover.test"
#define SRV_CNT         3
#define MAX_CONN        16

/* Transaction timers for the test: timer F (timeout) is TD */
#define TEST_T1         100
#define TEST_T2         400
#define TEST_T4         500
#define TEST_TD         1500

enum srv_mode
{
    MODE_OK,            /* Answer 200 */
    MODE_503,           /* Answer 503 */
    MODE_503_RETRY,     /* Answer 503 with Retry-After */
    MODE_SILENT,        /* Never answer */
    MODE_TRYING,        /* Answer 100, then nothing */
    MODE_CLOSED,        /* Nothing listens on the port */
    MODE_HANGING        /* TCP connections hang, the backlog is full */
};

struct fake_srv
{
    char                name[8];
    volatile int        mode;
    pj_sock_t           udp;
    pj_sock_t           tcp;
    pj_uint16_t         udp_port;
    pj_uint16_t         tcp_port;
    pj_sock_t           conn[MAX_CONN];
    pj_sock_t           filler[MAX_CONN];
    volatile int        hits;
};

static struct
{
    pj_pool_t          *pool;
    pj_mutex_t         *mutex;
    pj_thread_t        *thread;
    volatile pj_bool_t  quit;
    struct fake_srv     srv[SRV_CNT];
    pj_dns_resolver    *resolver;
    unsigned            refusal_msec;   /* How long a refusal takes */
} g;

static struct
{
    volatile pj_bool_t  done;
    int                 status;
    pj_bool_t           bad_token;
} result;

/* The token is allocated from the pool of the request, as PJSUA does */
#define TOKEN_MAGIC     0x5EF0A11


static pj_bool_t has_tag(const char *start, const char *end)
{
    const char *p;

    for (p = start; p + 5 <= end; ++p) {
        if (!pj_ansi_strnicmp(p, ";tag=", 5))
            return PJ_TRUE;
    }
    return PJ_FALSE;
}

/* Append to the response, which is cut at the buffer size */
static void append(char *buf, int size, int *len, const char *fmt, ...)
{
    va_list arg;
    int n;

    if (*len >= size - 1)
        return;
    va_start(arg, fmt);
    n = pj_ansi_vsnprintf(buf + *len, size - *len, fmt, arg);
    va_end(arg);
    *len = (n < 0 || n >= size - *len) ? size - 1 : *len + n;
}

/* Build a response to the request, copying the headers RFC 3261 requires */
static int build_response(const char *req, int code, const char *name,
                          const char *extra, char *buf, int size)
{
    static const char *copy[] = { "via:", "v:", "from:", "f:", "to:", "t:",
                                  "call-id:", "i:", "cseq:" };
    const char *reason = code == 100 ? "Trying" :
                         code == 200 ? "OK" : "Service Unavailable";
    const char *line = strstr(req, "\r\n");
    int len = 0;

    append(buf, size, &len, "SIP/2.0 %d %s\r\n", code, reason);
    while (line && line[2] != '\r' && line[2] != '\0') {
        const char *start = line + 2;
        const char *end = strstr(start, "\r\n");
        unsigned i;

        if (!end)
            break;
        for (i = 0; i < PJ_ARRAY_SIZE(copy); ++i) {
            if (pj_ansi_strnicmp(start, copy[i], pj_ansi_strlen(copy[i])))
                continue;
            append(buf, size, &len, "%.*s", (int)(end - start), start);
            /* Entries 4 and 5 are the To header */
            if ((i == 4 || i == 5) && code != 100 && !has_tag(start, end))
                append(buf, size, &len, ";tag=%s", name);
            append(buf, size, &len, "\r\n");
            break;
        }
        line = end;
    }
    append(buf, size, &len, "%sContent-Length: 0\r\n\r\n", extra);
    return len;
}

/* Handle a received message, return the response length or 0 */
static int handle_msg(struct fake_srv *srv, const char *msg, char *resp,
                      int size)
{
    int code;

    if (!pj_ansi_strncmp(msg, "SIP/2.0", 7) || !pj_ansi_strncmp(msg, "ACK", 3))
        return 0;

    ++srv->hits;

    switch (srv->mode) {
    case MODE_OK:       code = 200; break;
    case MODE_503:
    case MODE_503_RETRY: code = 503; break;
    case MODE_TRYING:   code = 100; break;
    default:            return 0;
    }

    return build_response(msg, code, srv->name,
                          srv->mode == MODE_503_RETRY ?
                              "Retry-After: 60\r\n" : "",
                          resp, size);
}

static int server_thread(void *arg)
{
    char buf[4000], resp[4000];

    PJ_UNUSED_ARG(arg);

    while (!g.quit) {
        pj_fd_set_t rset;
        pj_time_val timeout = { 0, 10 };
        pj_sock_t max_fd = 0;
        pj_bool_t idle;
        unsigned i, j;
        int rc = 0;

        /* The mutex is held until the reads are done, so set_mode() never
         * closes a socket in use: Windows forbids it, and may give the
         * handle of a closed socket to a new one at once.
         */
        PJ_FD_ZERO(&rset);
        pj_mutex_lock(g.mutex);
        for (i = 0; i < SRV_CNT; ++i) {
            struct fake_srv *srv = &g.srv[i];

            if (srv->udp != PJ_INVALID_SOCKET) {
                PJ_FD_SET(srv->udp, &rset);
                if (srv->udp > max_fd) max_fd = srv->udp;
            }
            if (srv->tcp != PJ_INVALID_SOCKET && srv->mode != MODE_HANGING) {
                PJ_FD_SET(srv->tcp, &rset);
                if (srv->tcp > max_fd) max_fd = srv->tcp;
            }
            for (j = 0; j < MAX_CONN; ++j) {
                if (srv->conn[j] != PJ_INVALID_SOCKET) {
                    PJ_FD_SET(srv->conn[j], &rset);
                    if (srv->conn[j] > max_fd) max_fd = srv->conn[j];
                }
            }
        }

        /* Windows fails select() on an empty set */
        idle = PJ_FD_COUNT(&rset) == 0;
        if (!idle)
            rc = pj_sock_select((int)max_fd + 1, &rset, NULL, NULL, &timeout);

        for (i = 0; rc > 0 && i < SRV_CNT; ++i) {
            struct fake_srv *srv = &g.srv[i];

            if (srv->udp != PJ_INVALID_SOCKET &&
                PJ_FD_ISSET(srv->udp, &rset))
            {
                pj_sockaddr src;
                int src_len = sizeof(src);
                pj_ssize_t len = sizeof(buf) - 1;

                if (pj_sock_recvfrom(srv->udp, buf, &len, 0, &src,
                                     &src_len) == PJ_SUCCESS && len > 0)
                {
                    pj_ssize_t rlen;

                    buf[len] = '\0';
                    rlen = handle_msg(srv, buf, resp, sizeof(resp));
                    if (rlen > 0)
                        pj_sock_sendto(srv->udp, resp, &rlen, 0, &src,
                                       src_len);
                }
            }

            for (j = 0; j < MAX_CONN; ++j) {
                pj_ssize_t len = sizeof(buf) - 1;

                if (srv->conn[j] == PJ_INVALID_SOCKET ||
                    !PJ_FD_ISSET(srv->conn[j], &rset))
                {
                    continue;
                }
                if (pj_sock_recv(srv->conn[j], buf, &len, 0) != PJ_SUCCESS ||
                    len <= 0)
                {
                    pj_sock_close(srv->conn[j]);
                    srv->conn[j] = PJ_INVALID_SOCKET;
                    continue;
                }
                buf[len] = '\0';
                if (buf[0] != '\r') {
                    pj_ssize_t rlen = handle_msg(srv, buf, resp,
                                                 sizeof(resp));
                    if (rlen > 0)
                        pj_sock_send(srv->conn[j], resp, &rlen, 0);
                }
            }

            /* After the reads, as the new connection may have the handle
             * of one closed above
             */
            if (srv->tcp != PJ_INVALID_SOCKET && srv->mode != MODE_HANGING &&
                PJ_FD_ISSET(srv->tcp, &rset))
            {
                pj_sock_t c;

                if (pj_sock_accept(srv->tcp, &c, NULL, NULL)==PJ_SUCCESS) {
                    for (j = 0; j < MAX_CONN; ++j) {
                        if (srv->conn[j] == PJ_INVALID_SOCKET) {
                            srv->conn[j] = c;
                            break;
                        }
                    }
                    if (j == MAX_CONN)
                        pj_sock_close(c);
                }
            }
        }
        pj_mutex_unlock(g.mutex);

        /* Let set_mode() take the mutex */
        pj_thread_sleep(idle ? 10 : 1);
    }

    return 0;
}

static pj_status_t open_sock(int type, int backlog, pj_uint16_t *port,
                             pj_sock_t *sock)
{
    pj_sockaddr_in addr;
    int addr_len = sizeof(addr);
    int reuse = 1;
    pj_str_t lo = pj_str("127.0.0.1");
    pj_status_t status;

    status = pj_sock_socket(pj_AF_INET(), type, 0, sock);
    if (status != PJ_SUCCESS)
        return status;

    pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), pj_SO_REUSEADDR(), &reuse,
                       sizeof(reuse));
    pj_sockaddr_in_init(&addr, &lo, *port);
    status = pj_sock_bind(*sock, &addr, sizeof(addr));
    if (status == PJ_SUCCESS && type == pj_SOCK_STREAM())
        status = pj_sock_listen(*sock, backlog);
    if (status == PJ_SUCCESS)
        status = pj_sock_getsockname(*sock, &addr, &addr_len);
    if (status != PJ_SUCCESS) {
        pj_sock_close(*sock);
        *sock = PJ_INVALID_SOCKET;
        return status;
    }

    *port = pj_sockaddr_in_get_port(&addr);
    return PJ_SUCCESS;
}

/* Windows refuses a connection to a closed port only after retrying it for
 * about two seconds.
 */
static unsigned get_refusal_delay(void)
{
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr_in addr;
    pj_uint16_t port = 0;
    pj_sock_t sock;
    pj_timestamp t0, t1;

    if (open_sock(pj_SOCK_STREAM(), 1, &port, &sock) != PJ_SUCCESS)
        return 0;
    pj_sock_close(sock);
    if (pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock) !=
        PJ_SUCCESS)
    {
        return 0;
    }
    pj_sockaddr_in_init(&addr, &lo, port);
    pj_get_timestamp(&t0);
    pj_sock_connect(sock, &addr, sizeof(addr));
    pj_get_timestamp(&t1);
    pj_sock_close(sock);
    return pj_elapsed_msec(&t0, &t1);
}

/* Set the server mode, closing or reopening its sockets as needed */
static pj_status_t set_mode(struct fake_srv *srv, int mode)
{
    pj_status_t status = PJ_SUCCESS;
    unsigned j;

    pj_mutex_lock(g.mutex);

    /* Drop the connections of the previous case */
    for (j = 0; j < MAX_CONN; ++j) {
        if (srv->conn[j] != PJ_INVALID_SOCKET) {
            pj_sock_close(srv->conn[j]);
            srv->conn[j] = PJ_INVALID_SOCKET;
        }
        if (srv->filler[j] != PJ_INVALID_SOCKET) {
            pj_sock_close(srv->filler[j]);
            srv->filler[j] = PJ_INVALID_SOCKET;
        }
    }

    if (mode == MODE_CLOSED) {
        if (srv->udp != PJ_INVALID_SOCKET) {
            pj_sock_close(srv->udp);
            srv->udp = PJ_INVALID_SOCKET;
        }
        if (srv->tcp != PJ_INVALID_SOCKET) {
            pj_sock_close(srv->tcp);
            srv->tcp = PJ_INVALID_SOCKET;
        }
    } else {
        if (srv->udp == PJ_INVALID_SOCKET)
            status = open_sock(pj_SOCK_DGRAM(), 0, &srv->udp_port, &srv->udp);
        if (status == PJ_SUCCESS && srv->tcp == PJ_INVALID_SOCKET)
            status = open_sock(pj_SOCK_STREAM(), 8, &srv->tcp_port, &srv->tcp);
        if (status == PJ_SUCCESS && srv->mode == MODE_HANGING)
            status = pj_sock_listen(srv->tcp, 8);
    }

    /* Fill the backlog, which is never accepted, so that the next
     * connection attempt hangs as if the server were unreachable. On Linux
     * a zero backlog holds one connection.
     */
    if (status == PJ_SUCCESS && mode == MODE_HANGING) {
        pj_str_t lo = pj_str("127.0.0.1");
        pj_sockaddr_in addr;

        pj_sockaddr_in_init(&addr, &lo, srv->tcp_port);
        status = pj_sock_listen(srv->tcp, 0);
        if (status == PJ_SUCCESS)
            status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0,
                                    &srv->filler[0]);
        if (status == PJ_SUCCESS)
            status = pj_sock_connect(srv->filler[0], &addr, sizeof(addr));
    }

    srv->mode = mode;
    srv->hits = 0;
    pj_mutex_unlock(g.mutex);
    return status;
}

static void init_query(pj_dns_parsed_packet *pkt, pj_dns_parsed_query *q,
                       pj_dns_type type, const pj_str_t *name)
{
    pj_bzero(q, sizeof(*q));
    q->type = type;
    q->dnsclass = PJ_DNS_CLASS_IN;
    q->name = *name;

    pj_bzero(pkt, sizeof(*pkt));
    pkt->hdr.flags = PJ_DNS_SET_QR(1);
    pkt->hdr.qdcount = 1;
    pkt->q = q;
}

/* Put the answers in the resolver cache, never expiring, so no DNS server
 * is needed.
 */
static pj_status_t add_dns_records(void)
{
    pj_str_t srv_name[2] = { { "_sip._udp." TEST_DOMAIN, 0 },
                             { "_sip._tcp." TEST_DOMAIN, 0 } };
    pj_dns_parsed_rr rr[SRV_CNT];
    pj_dns_parsed_query q;
    pj_dns_parsed_packet pkt;
    pj_str_t target[SRV_CNT];
    char target_buf[SRV_CNT][32];
    pj_in_addr lo = pj_inet_addr2("127.0.0.1");
    unsigned i, t;
    pj_status_t status;

    for (i = 0; i < SRV_CNT; ++i) {
        pj_ansi_snprintf(target_buf[i], sizeof(target_buf[i]),
                         "srv%d." TEST_DOMAIN, i + 1);
        target[i] = pj_str(target_buf[i]);
    }

    for (t = 0; t < 2; ++t) {
        srv_name[t].slen = pj_ansi_strlen(srv_name[t].ptr);
        for (i = 0; i < SRV_CNT; ++i) {
            pj_dns_init_srv_rr(&rr[i], &srv_name[t], PJ_DNS_CLASS_IN, 60,
                               i + 1, 5,
                               t ? g.srv[i].tcp_port : g.srv[i].udp_port,
                               &target[i]);
        }
        init_query(&pkt, &q, PJ_DNS_TYPE_SRV, &srv_name[t]);
        pkt.hdr.anscount = SRV_CNT;
        pkt.ans = rr;
        status = pj_dns_resolver_add_entry(g.resolver, &pkt, PJ_FALSE);
        if (status != PJ_SUCCESS)
            return status;
    }

    for (i = 0; i < SRV_CNT; ++i) {
        pj_dns_init_a_rr(&rr[0], &target[i], PJ_DNS_CLASS_IN, 60, &lo);
        init_query(&pkt, &q, PJ_DNS_TYPE_A, &target[i]);
        pkt.hdr.anscount = 1;
        pkt.ans = rr;
        status = pj_dns_resolver_add_entry(g.resolver, &pkt, PJ_FALSE);
        if (status != PJ_SUCCESS)
            return status;

        /* No IPv6 address, for builds that also look up AAAA records */
        init_query(&pkt, &q, PJ_DNS_TYPE_AAAA, &target[i]);
        status = pj_dns_resolver_add_entry(g.resolver, &pkt, PJ_FALSE);
        if (status != PJ_SUCCESS)
            return status;
    }

    return PJ_SUCCESS;
}

static void send_cb(void *token, pjsip_event *e)
{
    if (*(unsigned*)token != TOKEN_MAGIC)
        result.bad_token = PJ_TRUE;
    if (e->type == PJSIP_EVENT_TSX_STATE && e->body.tsx_state.tsx)
        result.status = e->body.tsx_state.tsx->status_code;
    result.done = PJ_TRUE;
}

struct test_case
{
    const char *title;
    pj_bool_t   tcp;
    int         mode[SRV_CNT];
    int         status;         /* Expected final status */
    pj_bool_t   reached[SRV_CNT];
    pj_bool_t   keep_failed;    /* Keep the failed servers of the previous
                                   case */
    pj_bool_t   srv1_last;      /* The resolver must now list srv1 last */
    pj_bool_t   keep_srv1_only; /* Keep only whether srv1 has failed */
    pj_bool_t   no_failover;    /* Disable server_failover */
    pj_bool_t   srv1_first;     /* The resolver must still list srv1 first */
    pj_bool_t   pin_srv1;       /* Send on a connection to srv1 */
    pj_bool_t   late_clear;     /* Clear the failed servers after sending */
};

/* Forget the servers that failed in the previous cases */
static void forget_failed_servers(unsigned first)
{
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    unsigned i;

    for (i = first; i < SRV_CNT; ++i) {
        pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[i].udp_port);
        pjsip_endpt_set_server_failed(endpt, PJSIP_TRANSPORT_UDP, &addr, 0);
        pj_sockaddr_set_port(&addr, g.srv[i].tcp_port);
        pjsip_endpt_set_server_failed(endpt, PJSIP_TRANSPORT_TCP, &addr, 0);
    }
}

static struct
{
    volatile pj_bool_t      done;
    pj_status_t             status;
    pjsip_server_addresses  addr;
} resolved;

static void resolve_cb(pj_status_t status, void *token,
                       const struct pjsip_server_addresses *addr)
{
    PJ_UNUSED_ARG(token);

    resolved.status = status;
    if (status == PJ_SUCCESS)
        resolved.addr = *addr;
    resolved.done = PJ_TRUE;
}

/* Check whether the resolver lists srv1 last, as it does for any user of
 * the resolver, e.g: an INVITE session, or first.
 */
static int check_srv1_order(pj_bool_t tcp, pj_bool_t last)
{
    pjsip_host_info target;
    unsigned waited, srv1_pos;

    pj_bzero(&target, sizeof(target));
    target.type = tcp ? PJSIP_TRANSPORT_TCP : PJSIP_TRANSPORT_UDP;
    target.flag = pjsip_transport_get_flag_from_type(target.type);
    target.addr.host = pj_str(TEST_DOMAIN);

    pj_bzero(&resolved, sizeof(resolved));
    pjsip_endpt_resolve(endpt, g.pool, &target, NULL, &resolve_cb);
    for (waited = 0; !resolved.done && waited < 2000; waited += 50)
        flush_events(50);

    PJ_TEST_TRUE(resolved.done, NULL, return -3060);
    PJ_TEST_SUCCESS(resolved.status, NULL, return -3061);
    PJ_TEST_EQ(resolved.addr.count, SRV_CNT, NULL, return -3062);
    srv1_pos = last ? SRV_CNT - 1 : 0;
    PJ_TEST_EQ(pj_sockaddr_get_port(&resolved.addr.entry[srv1_pos].addr),
               tcp ? g.srv[0].tcp_port : g.srv[0].udp_port, NULL,
               return -3063);
    return 0;
}

static int run_case(const struct test_case *tc)
{
    char target_buf[64];
    pj_str_t target, from = pj_str("<sip:tester@127.0.0.1>");
    pjsip_tx_data *tdata;
    pjsip_transport *tp = NULL;
    unsigned *token;
    unsigned i, waited, td = TEST_TD;
    int rc = 0;
    pj_status_t status;

    for (i = 0; i < SRV_CNT; ++i) {
        status = set_mode(&g.srv[i], tc->mode[i]);
        if (status != PJ_SUCCESS) {
            app_perror("    error: setting server mode", status);
            return -3010;
        }
        /* A refused connection moves on before timer F */
        if (tc->tcp && tc->mode[i] == MODE_CLOSED)
            td = TEST_TD + 2 * g.refusal_msec;
    }
    pjsip_tsx_set_timers(TEST_T1, TEST_T2, TEST_T4, td);
    /* Let the transports of the previous case see the closed connections */
    flush_events(200);
    if (!tc->keep_failed)
        forget_failed_servers(0);
    else if (tc->keep_srv1_only)
        forget_failed_servers(1);

    pj_ansi_snprintf(target_buf, sizeof(target_buf), "sip:" TEST_DOMAIN "%s",
                     tc->tcp ? ";transport=tcp" : "");
    target = pj_str(target_buf);

    status = pjsip_endpt_create_request(endpt, &pjsip_options_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("    error: creating request", status);
        return -3020;
    }

    if (tc->pin_srv1) {
        pj_str_t lo = pj_str("127.0.0.1");
        pjsip_tpselector sel;
        pj_sockaddr addr;

        pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[0].tcp_port);
        status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP,
                                               &addr, sizeof(pj_sockaddr_in),
                                               NULL, &tp);
        if (status != PJ_SUCCESS) {
            app_perror("    error: connecting to srv1", status);
            pjsip_tx_data_dec_ref(tdata);
            return -3025;
        }
        pj_bzero(&sel, sizeof(sel));
        sel.type = PJSIP_TPSELECTOR_TRANSPORT;
        sel.u.transport = tp;
        pjsip_tx_data_set_transport(tdata, &sel);
    }

    token = PJ_POOL_ALLOC_T(tdata->pool, unsigned);
    *token = TOKEN_MAGIC;

    pjsip_cfg()->endpt.server_failover = !tc->no_failover;
    pj_bzero(&result, sizeof(result));
    status = pjsip_endpt_send_request(endpt, tdata, -1, token, &send_cb);
    if (status != PJ_SUCCESS) {
        app_perror("    error: sending request", status);
        return -3030;
    }
    if (tc->late_clear)
        pjsip_endpt_clear_failed_servers(endpt);

    for (waited = 0; !result.done && waited < 4 * td + 2000;
         waited += 50)
    {
        flush_events(50);
    }
    if (tp)
        pjsip_transport_dec_ref(tp);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;

    PJ_LOG(3,(THIS_FILE, "  %-44s -> %d, reached %s%s%s", tc->title,
              result.done ? result.status : -1,
              g.srv[0].hits ? "1" : "-", g.srv[1].hits ? "2" : "-",
              g.srv[2].hits ? "3" : "-"));

    if (!result.done || result.status != tc->status)
        rc = -3040;
    if (result.bad_token) {
        PJ_LOG(1,(THIS_FILE, "    error: the callback got a bad token"));
        return -3045;
    }
    for (i = 0; i < SRV_CNT; ++i) {
        if ((g.srv[i].hits != 0) != tc->reached[i])
            rc = -3050;
    }
    if (rc == 0 && tc->pin_srv1 && g.srv[0].hits != 1) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 got %d requests", g.srv[0].hits));
        return -3059;
    }
    if (rc == 0 && tc->srv1_first && check_srv1_order(tc->tcp, PJ_FALSE)) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 is not listed first"));
        return -3057;
    }
    if (rc == 0 && tc->srv1_last && check_srv1_order(tc->tcp, PJ_TRUE)) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 is not listed last"));
        return -3055;
    }
    if (rc) {
        PJ_LOG(1,(THIS_FILE, "    error: expected %d, reached %s%s%s",
                  tc->status, tc->reached[0] ? "1" : "-",
                  tc->reached[1] ? "2" : "-", tc->reached[2] ? "3" : "-"));
    }
    return rc;
}

static const struct test_case cases[] =
{
    { "UDP, srv1 answers",
      PJ_FALSE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE } },
    { "UDP, srv1 never answers: it is remembered",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "UDP, next request goes to srv2",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_TRUE },
    { "UDP, srv2 never answers",
      PJ_FALSE, { MODE_OK, MODE_SILENT, MODE_OK }, 408,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_TRUE },
    { "UDP, srv3 never answers",
      PJ_FALSE, { MODE_OK, MODE_OK, MODE_SILENT }, 408,
      { PJ_FALSE, PJ_FALSE, PJ_TRUE }, PJ_TRUE },
    { "UDP, all failed: srv1 is tried, then forgotten",
      PJ_FALSE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_TRUE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 answers 503: not remembered",
      PJ_FALSE, { MODE_503, MODE_OK, MODE_OK }, 503,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 answers 503 with Retry-After",
      PJ_FALSE, { MODE_503_RETRY, MODE_OK, MODE_OK }, 503,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 answers 100 only: not remembered",
      PJ_FALSE, { MODE_TRYING, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_TRUE },
    { "UDP, disabled: srv1 is not remembered",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_TRUE, PJ_TRUE },
#if PJ_HAS_TCP
    { "TCP, srv1 answers",
      PJ_TRUE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE } },
    { "TCP, srv1 port closed",
      PJ_TRUE, { MODE_CLOSED, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "TCP, next request goes to srv2",
      PJ_TRUE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_TRUE },
    { "TCP, disabled: closed srv1 is not remembered",
      PJ_TRUE, { MODE_CLOSED, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_TRUE, PJ_TRUE },
    { "TCP, srv1 never answers",
      PJ_TRUE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "TCP, connection to srv1: srv1 is remembered",
      PJ_TRUE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_TRUE },
#endif
};

/* A refused connection is remembered only when another server answers:
 * when none does, the local network may be the cause.
 */
static int refused_test(void)
{
    static const struct test_case tc[] =
    {
        { "TCP, srv1 closed, srv2 answers: srv1 is remembered",
          PJ_TRUE, { MODE_CLOSED, MODE_OK, MODE_OK }, 200,
          { PJ_FALSE, PJ_TRUE, PJ_FALSE } },
        { "TCP, all ports closed: nothing is remembered",
          PJ_TRUE, { MODE_CLOSED, MODE_CLOSED, MODE_CLOSED }, 503,
          { PJ_FALSE, PJ_FALSE, PJ_FALSE } },
    };
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    unsigned i;
    int rc = 0;

    for (i = 0; i < PJ_ARRAY_SIZE(tc) && rc == 0; ++i) {
        unsigned j;

        rc = run_case(&tc[i]);
        /* The first case remembers srv1 only, the second one none */
        for (j = 0; rc == 0 && j < SRV_CNT; ++j) {
            pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[j].tcp_port);
            if (pjsip_endpt_is_server_failed(endpt, PJSIP_TRANSPORT_TCP,
                                             &addr) != (i == 0 && j == 0))
            {
                PJ_LOG(1,(THIS_FILE, "    error: srv%d is %sremembered",
                          j + 1, (i == 0 && j == 0) ? "not " : ""));
                rc = -3071;
            }
        }
    }
    return rc;
}

/* A request sent before the failed servers are cleared, e.g: on the previous
 * network, is not sent to another server, and its failure is not remembered.
 */
static int late_clear_test(void)
{
    struct test_case tc;

    pj_bzero(&tc, sizeof(tc));
    tc.title = "UDP, sent before the failed servers are cleared";
    tc.mode[0] = MODE_SILENT;
    tc.status = 408;
    tc.reached[0] = PJ_TRUE;
    tc.srv1_first = PJ_TRUE;
    tc.late_clear = PJ_TRUE;
    return run_case(&tc);
}

/* A 503 is remembered for its Retry-After, up to failed_server_timeout */
static int retry_after_limit_test(void)
{
    static const struct test_case tc =
    { "UDP, Retry-After longer than failed_server_timeout",
      PJ_FALSE, { MODE_503_RETRY, MODE_OK, MODE_OK }, 503,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE };
    unsigned saved = pjsip_cfg()->endpt.failed_server_timeout;
    unsigned waited;
    int rc;

    pjsip_cfg()->endpt.failed_server_timeout = 1;
    rc = run_case(&tc);
    for (waited = 0; rc == 0 && waited < 1500; waited += 100)
        flush_events(100);
    if (rc == 0 && check_srv1_order(PJ_FALSE, PJ_FALSE) != 0) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 is still remembered"));
        rc = -3058;
    }
    pjsip_cfg()->endpt.failed_server_timeout = saved;
    return rc;
}

#if PJ_HAS_TCP && defined(PJ_LINUX) && PJ_LINUX
/* Only Linux drops the connection requests beyond the backlog. This is the
 * last case: the connection to srv1 stays pending, and would reach srv1 once
 * it listens again.
 */
static int hanging_test(void)
{
    static const struct test_case tc =
    { "TCP, srv1 connection hangs",
      PJ_TRUE, { MODE_HANGING, MODE_OK, MODE_OK }, 408,
      { PJ_FALSE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE };

    return run_case(&tc);
}
#endif

/* Marking and clearing a failed server from the application */
static int failed_server_api_test(void)
{
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    int rc;

    PJ_LOG(3,(THIS_FILE, "  failed server API"));

    forget_failed_servers(0);
    pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[0].udp_port);

    PJ_TEST_SUCCESS(pjsip_endpt_set_server_failed(endpt, PJSIP_TRANSPORT_UDP,
                                                  &addr, 60),
                    NULL, return -3080);
    PJ_TEST_TRUE(pjsip_endpt_is_server_failed(endpt, PJSIP_TRANSPORT_UDP,
                                              &addr), NULL, return -3081);
    /* The mark is for the transport type too */
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, PJSIP_TRANSPORT_TCP,
                                               &addr), NULL, return -3082);
    if ((rc = check_srv1_order(PJ_FALSE, PJ_TRUE)) != 0)
        return rc - 10;

    PJ_TEST_SUCCESS(pjsip_endpt_set_server_failed(endpt, PJSIP_TRANSPORT_UDP,
                                                  &addr, 0),
                    NULL, return -3083);
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, PJSIP_TRANSPORT_UDP,
                                               &addr), NULL, return -3084);
    if ((rc = check_srv1_order(PJ_FALSE, PJ_FALSE)) != 0)
        return rc - 20;

    pjsip_endpt_set_server_failed(endpt, PJSIP_TRANSPORT_UDP, &addr, 60);
    PJ_TEST_SUCCESS(pjsip_endpt_clear_failed_servers(endpt), NULL,
                    return -3085);
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, PJSIP_TRANSPORT_UDP,
                                               &addr), NULL, return -3086);
    rc = check_srv1_order(PJ_FALSE, PJ_FALSE);
    return rc ? rc - 30 : 0;
}

static void destroy(void)
{
    unsigned i;

    if (g.thread) {
        g.quit = PJ_TRUE;
        pj_thread_join(g.thread);
        pj_thread_destroy(g.thread);
    }
    for (i = 0; i < SRV_CNT; ++i) {
        set_mode(&g.srv[i], MODE_CLOSED);
    }
    if (g.resolver) {
        pjsip_endpt_set_resolver(endpt, NULL);
        pj_dns_resolver_destroy(g.resolver, PJ_FALSE);
    }
    if (g.mutex)
        pj_mutex_destroy(g.mutex);
    if (g.pool)
        pj_pool_release(g.pool);
    pj_bzero(&g, sizeof(g));
}

int srv_failover_test(void)
{
    pjsip_cfg_t saved_cfg = *pjsip_cfg();
    /* Nothing listens there: all the answers come from the cache */
    pj_str_t ns = pj_str("127.0.0.1");
    pj_uint16_t dns_port = 9;
    unsigned i;
    int rc = 0, failed = 0;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "DNS SRV failover test"));

    pj_bzero(&g, sizeof(g));
    g.pool = pjsip_endpt_create_pool(endpt, "srvfo", 1000, 1000);
    PJ_TEST_SUCCESS(pj_mutex_create_simple(g.pool, "srvfo", &g.mutex),
                    NULL, { rc = -3001; goto on_return; });

    for (i = 0; i < SRV_CNT; ++i) {
        struct fake_srv *srv = &g.srv[i];
        unsigned j;

        pj_ansi_snprintf(srv->name, sizeof(srv->name), "srv%d", i + 1);
        srv->udp = srv->tcp = PJ_INVALID_SOCKET;
        for (j = 0; j < MAX_CONN; ++j)
            srv->conn[j] = srv->filler[j] = PJ_INVALID_SOCKET;
        PJ_TEST_SUCCESS(set_mode(srv, MODE_OK), NULL,
                        { rc = -3002; goto on_return; });
    }

    PJ_TEST_SUCCESS(pjsip_endpt_create_resolver(endpt, &g.resolver), NULL,
                    { rc = -3006; goto on_return; });
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(g.resolver, 1, &ns, &dns_port),
                    NULL, { rc = -3007; goto on_return; });
    PJ_TEST_SUCCESS(add_dns_records(), NULL, { rc = -3004; goto on_return; });
    PJ_TEST_SUCCESS(pjsip_endpt_set_resolver(endpt, g.resolver), NULL,
                    { rc = -3008; goto on_return; });

    status = pj_thread_create(g.pool, "srvfo", &server_thread, NULL, 0, 0,
                              &g.thread);
    PJ_TEST_SUCCESS(status, NULL, { rc = -3009; goto on_return; });

    g.refusal_msec = get_refusal_delay();
    PJ_LOG(3,(THIS_FILE, "  a closed port refuses in %u ms", g.refusal_msec));

    pjsip_tsx_set_timers(TEST_T1, TEST_T2, TEST_T4, TEST_TD);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;
    forget_failed_servers(0);

    /* Run all cases, so the log shows the whole picture */
    for (i = 0; i < PJ_ARRAY_SIZE(cases); ++i) {
        int case_rc = run_case(&cases[i]);
        if (case_rc) {
            ++failed;
            if (!rc)
                rc = case_rc;
        }
    }
    if (failed) {
        PJ_LOG(1,(THIS_FILE, "  %d of %d cases failed", failed,
                  (int)PJ_ARRAY_SIZE(cases)));
    }

    status = late_clear_test();
    if (status && !rc)
        rc = status;

#if PJ_HAS_TCP
    status = refused_test();
    if (status && !rc)
        rc = status;
#endif

    status = retry_after_limit_test();
    if (status && !rc)
        rc = status;

#if PJ_HAS_TCP && defined(PJ_LINUX) && PJ_LINUX
    status = hanging_test();
    if (status && !rc)
        rc = status;
#endif

    status = failed_server_api_test();
    if (status) {
        PJ_LOG(1,(THIS_FILE, "    error: failed server API [%d]", status));
        if (!rc)
            rc = status;
    }

on_return:
    pjsip_tsx_set_timers(saved_cfg.tsx.t1, saved_cfg.tsx.t2,
                         saved_cfg.tsx.t4, saved_cfg.tsx.td);
    pjsip_cfg()->endpt.server_failover = saved_cfg.endpt.server_failover;
    /* Let the transactions and connections of the last case finish */
    flush_events(2 * TEST_TD);
    destroy();
    return rc;
}

#else   /* PJSIP_HAS_RESOLVER && PJ_HAS_THREADS */

int srv_failover_test(void)
{
    return 0;
}

#endif  /* PJSIP_HAS_RESOLVER && PJ_HAS_THREADS */

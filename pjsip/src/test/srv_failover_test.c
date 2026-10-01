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
#include <pjsip_ua.h>
#include <pjlib-util.h>
#include <pjlib.h>

#define THIS_FILE   "srv_failover_test.c"

#if PJSIP_HAS_RESOLVER && PJ_HAS_THREADS

#define TEST_DOMAIN     "failover.test"
/* Its SRV targets list srv1 twice, with srv2 before or after the second */
#define DUP_DOMAIN      "dup." TEST_DOMAIN
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
    MODE_401,           /* Challenge, then answer 100 only */
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
    volatile int        tsx_cnt;        /* Requests with a new Via */
    volatile int        accepts;        /* Connections accepted */
    char                last_via[160];
};

static struct
{
    pj_pool_t          *pool;
    pj_mutex_t         *mutex;
    pj_thread_t        *thread;
    volatile pj_bool_t  quit;
    struct fake_srv     srv[SRV_CNT];
    pj_dns_resolver    *resolver;
    pj_dns_resolver    *prev_resolver;  /* The endpoint's, to restore */
    unsigned            refusal_msec;   /* How long a refusal takes */
#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT
    pjsip_tpfactory    *tls;
#endif
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
                         code == 200 ? "OK" :
                         code == 401 ? "Unauthorized" : "Service Unavailable";
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

    const char *via;

    /* Not a request, e.g: a response, or the start of a TLS handshake */
    if (!pj_ansi_strncmp(msg, "SIP/2.0", 7) ||
        !pj_ansi_strncmp(msg, "ACK", 3) || !strstr(msg, " SIP/2.0\r\n"))
    {
        return 0;
    }

    ++srv->hits;
    via = strstr(msg, "\r\nVia:");
    if (via) {
        const char *end = strstr(via + 2, "\r\n");
        char buf[sizeof(srv->last_via)];

        pj_ansi_snprintf(buf, sizeof(buf), "%.*s",
                         end ? (int)(end - via - 2) : 0, via + 2);
        if (pj_ansi_strcmp(buf, srv->last_via) != 0)
            ++srv->tsx_cnt;
        pj_ansi_strxcpy(srv->last_via, buf, sizeof(srv->last_via));
    }

    switch (srv->mode) {
    case MODE_OK:       code = 200; break;
    case MODE_503:
    case MODE_503_RETRY: code = 503; break;
    case MODE_TRYING:   code = 100; break;
    case MODE_401:      code = strstr(msg, "\r\nAuthorization:") ? 100 : 401;
                        break;
    default:            return 0;
    }

    return build_response(msg, code, srv->name,
                          srv->mode == MODE_503_RETRY ?
                              "Retry-After: 60\r\n" :
                          code == 401 ?
                              "WWW-Authenticate: Digest realm=\"" TEST_DOMAIN
                              "\", nonce=\"1\"\r\n" : "",
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
                    ++srv->accepts;
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
        if (status == PJ_SUCCESS && srv->tcp == PJ_INVALID_SOCKET) {
            /* On the UDP port, as servers usually are, when free */
            if (srv->tcp_port == 0)
                srv->tcp_port = srv->udp_port;
            status = open_sock(pj_SOCK_STREAM(), 8, &srv->tcp_port, &srv->tcp);
            if (status != PJ_SUCCESS && srv->tcp_port == srv->udp_port) {
                srv->tcp_port = 0;
                status = open_sock(pj_SOCK_STREAM(), 8, &srv->tcp_port,
                                   &srv->tcp);
            }
        }
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
    srv->tsx_cnt = 0;
    srv->accepts = 0;
    srv->last_via[0] = '\0';
    pj_mutex_unlock(g.mutex);
    return status;
}

/* Close the connections of a server, as if it went down */
static void drop_connections(struct fake_srv *srv)
{
    unsigned j;

    pj_mutex_lock(g.mutex);
    for (j = 0; j < MAX_CONN; ++j) {
        if (srv->conn[j] != PJ_INVALID_SOCKET) {
            pj_sock_close(srv->conn[j]);
            srv->conn[j] = PJ_INVALID_SOCKET;
        }
    }
    pj_mutex_unlock(g.mutex);
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
    pj_str_t srv_name[3] = { { "_sip._udp." TEST_DOMAIN, 0 },
                             { "_sip._tcp." TEST_DOMAIN, 0 },
                             { "_sips._tcp." TEST_DOMAIN, 0 } };
    pj_str_t dup_name = { "_sip._udp." DUP_DOMAIN, 0 };
    pj_str_t dup_tcp_name = { "_sip._tcp." DUP_DOMAIN, 0 };
    pj_dns_parsed_rr rr[SRV_CNT];
    pj_dns_parsed_query q;
    pj_dns_parsed_packet pkt;
    pj_str_t target[SRV_CNT + 1];
    char target_buf[SRV_CNT + 1][32];
    pj_in_addr lo = pj_inet_addr2("127.0.0.1");
    unsigned i, t;
    pj_status_t status;

    for (i = 0; i < SRV_CNT; ++i) {
        pj_ansi_snprintf(target_buf[i], sizeof(target_buf[i]),
                         "srv%d." TEST_DOMAIN, i + 1);
        target[i] = pj_str(target_buf[i]);
    }
    target[SRV_CNT] = pj_str("srv0." TEST_DOMAIN);
    dup_name.slen = pj_ansi_strlen(dup_name.ptr);
    dup_tcp_name.slen = pj_ansi_strlen(dup_tcp_name.ptr);

    /* TLS is served on the TCP port too */
    for (t = 0; t < 3; ++t) {
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

    /* UDP SRV targets of DUP_DOMAIN: srv1, srv1 again as "srv0", srv2 */
    pj_dns_init_srv_rr(&rr[0], &dup_name, PJ_DNS_CLASS_IN, 60, 1, 5,
                       g.srv[0].udp_port, &target[0]);
    pj_dns_init_srv_rr(&rr[1], &dup_name, PJ_DNS_CLASS_IN, 60, 2, 5,
                       g.srv[0].udp_port, &target[SRV_CNT]);
    pj_dns_init_srv_rr(&rr[2], &dup_name, PJ_DNS_CLASS_IN, 60, 3, 5,
                       g.srv[1].udp_port, &target[1]);
    init_query(&pkt, &q, PJ_DNS_TYPE_SRV, &dup_name);
    pkt.hdr.anscount = 3;
    pkt.ans = rr;
    status = pj_dns_resolver_add_entry(g.resolver, &pkt, PJ_FALSE);
    if (status != PJ_SUCCESS)
        return status;

    /* TCP SRV targets of DUP_DOMAIN: srv1, srv2, srv1 again as "srv0" */
    pj_dns_init_srv_rr(&rr[0], &dup_tcp_name, PJ_DNS_CLASS_IN, 60, 1, 5,
                       g.srv[0].tcp_port, &target[0]);
    pj_dns_init_srv_rr(&rr[1], &dup_tcp_name, PJ_DNS_CLASS_IN, 60, 2, 5,
                       g.srv[1].tcp_port, &target[1]);
    pj_dns_init_srv_rr(&rr[2], &dup_tcp_name, PJ_DNS_CLASS_IN, 60, 3, 5,
                       g.srv[0].tcp_port, &target[SRV_CNT]);
    init_query(&pkt, &q, PJ_DNS_TYPE_SRV, &dup_tcp_name);
    pkt.hdr.anscount = 3;
    pkt.ans = rr;
    status = pj_dns_resolver_add_entry(g.resolver, &pkt, PJ_FALSE);
    if (status != PJ_SUCCESS)
        return status;

    for (i = 0; i <= SRV_CNT; ++i) {
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
    pj_bool_t   hold_tsx;       /* Send with pjsip_endpt_send_request2(),
                                   keeping the transaction */
    pj_bool_t   nat_via;        /* Send with a public address in Via */
    const char *domain;         /* Send to this domain, not TEST_DOMAIN */
    pj_bool_t   late_mark_srv2; /* Mark srv2 as failed after sending */
    pj_bool_t   late_clear;     /* Clear the failed servers after sending */
    pj_bool_t   check_release;  /* The request must be released at the end */
    pj_bool_t   large;          /* Too large for UDP: sent with TCP first */
    pj_bool_t   tls;            /* Send with TLS: no handshake completes */
    pj_bool_t   drop_srv1;      /* Close the connections of srv1 once
                                   srv2 has got the request */
    pj_bool_t   clear_on_retry; /* Clear the failed servers once srv2 has
                                   got the request */
};

/* Forget the servers that failed in the previous cases */
static void forget_failed_servers(unsigned first)
{
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    unsigned i;

    for (i = first; i < SRV_CNT; ++i) {
        pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[i].udp_port);
        pjsip_endpt_set_server_failed(endpt, &addr, 0);
        pj_sockaddr_set_port(&addr, g.srv[i].tcp_port);
        pjsip_endpt_set_server_failed(endpt, &addr, 0);
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
    pjsip_transaction *tsx = NULL;
    pjsip_transport *tp = NULL;
    unsigned *token;
    unsigned i, waited, td = TEST_TD;
    pj_bool_t dropped = PJ_FALSE, cleared = PJ_FALSE;
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

    pj_ansi_snprintf(target_buf, sizeof(target_buf), "sip:%s%s",
                     tc->domain ? tc->domain : TEST_DOMAIN,
                     tc->tls ? ";transport=tls" :
                     tc->tcp ? ";transport=tcp" : "");
    target = pj_str(target_buf);

    status = pjsip_endpt_create_request(endpt, &pjsip_options_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("    error: creating request", status);
        return -3020;
    }

    if (tc->nat_via) {
        tdata->via_addr.host = pj_str("192.0.2.1");
        tdata->via_addr.port = 5099;
    }

    if (tc->large) {
        pj_str_t name = pj_str("X-Padding");
        pj_str_t value;

        value.ptr = (char*) pj_pool_alloc(tdata->pool,
                                          PJSIP_UDP_SIZE_THRESHOLD);
        value.slen = PJSIP_UDP_SIZE_THRESHOLD;
        pj_memset(value.ptr, 'x', value.slen);
        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
                          pjsip_generic_string_hdr_create(tdata->pool, &name,
                                                          &value));
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

    if (tc->check_release)
        pjsip_tx_data_add_ref(tdata);

    pjsip_cfg()->endpt.server_failover = !tc->no_failover;
    pj_bzero(&result, sizeof(result));
    status = pjsip_endpt_send_request2(endpt, tdata, -1, token, &send_cb,
                                       tc->hold_tsx ? &tsx : NULL);
    if (status != PJ_SUCCESS) {
        app_perror("    error: sending request", status);
        if (tc->check_release)
            pjsip_tx_data_dec_ref(tdata);
        if (tp)
            pjsip_transport_dec_ref(tp);
        return -3030;
    }
    if (tc->late_clear)
        pjsip_endpt_clear_failed_servers(endpt);
    if (tc->late_mark_srv2) {
        pj_str_t lo = pj_str("127.0.0.1");
        pj_sockaddr addr;

        pj_sockaddr_init(pj_AF_INET(), &addr, &lo,
                         tc->tcp ? g.srv[1].tcp_port : g.srv[1].udp_port);
        pjsip_endpt_set_server_failed(endpt, &addr, 60);
    }

    for (waited = 0; !result.done && waited < 4 * td + 2000;
         waited += 50)
    {
        flush_events(50);
        if (g.srv[1].hits == 0 && g.srv[1].accepts == 0)
            continue;
        if (tc->drop_srv1 && !dropped) {
            drop_connections(&g.srv[0]);
            dropped = PJ_TRUE;
        }
        if (tc->clear_on_retry && !cleared) {
            pjsip_endpt_clear_failed_servers(endpt);
            cleared = PJ_TRUE;
        }
    }
    if ((tc->drop_srv1 && !dropped) || (tc->clear_on_retry && !cleared)) {
        PJ_LOG(1,(THIS_FILE, "    error: done before srv2 got the request"));
        rc = -3047;
    }
    if (tsx)
        pj_grp_lock_dec_ref(tsx->grp_lock);
    if (tp)
        pjsip_transport_dec_ref(tp);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;

    if (tc->check_release) {
        /* Only our reference is left once the transactions are gone */
        for (waited = 0; pj_atomic_get(tdata->ref_cnt) > 1 && waited < 3000;
             waited += 50)
        {
            flush_events(50);
        }
        if (pj_atomic_get(tdata->ref_cnt) > 1) {
            PJ_LOG(1,(THIS_FILE, "    error: the request was not released"));
            rc = -3046;
        }
        pjsip_tx_data_dec_ref(tdata);
    }

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
        /* No server gets the request twice */
        if (g.srv[i].tsx_cnt > 1) {
            PJ_LOG(1,(THIS_FILE, "    error: srv%d got %d transactions",
                      i + 1, g.srv[i].tsx_cnt));
            rc = -3051;
        }
    }
    if (rc == 0 && tc->pin_srv1 && g.srv[0].hits != 1) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 got %d requests", g.srv[0].hits));
        return -3059;
    }
    if (rc == 0 && tc->nat_via &&
        !strstr(g.srv[1].last_via, "192.0.2.1:5099"))
    {
        PJ_LOG(1,(THIS_FILE, "    error: srv2 got %s", g.srv[1].last_via));
        return -3056;
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
    { "UDP, srv1 never answers",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "UDP, next request skips the failed srv1",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_TRUE },
    { "UDP, the failed srv1 is tried last",
      PJ_FALSE, { MODE_OK, MODE_503, MODE_503 }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_TRUE }, PJ_TRUE },
    { "UDP, srv1 silent, the others 503 with Retry-After",
      PJ_FALSE, { MODE_SILENT, MODE_503_RETRY, MODE_503_RETRY }, 503,
      { PJ_TRUE, PJ_TRUE, PJ_TRUE }, PJ_TRUE },
    { "UDP, all failed: the first one is tried",
      PJ_FALSE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_TRUE },
    { "UDP, srv1 is first again after answering",
      PJ_FALSE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_TRUE, PJ_FALSE, PJ_TRUE },
    { "UDP, only failed servers left: they are tried",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_TRUE, PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 answers 503 (not remembered)",
      PJ_FALSE, { MODE_503, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 answers 503 with Retry-After",
      PJ_FALSE, { MODE_503_RETRY, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "UDP, the retry keeps the public address in Via",
      PJ_FALSE, { MODE_503, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_TRUE },
    { "UDP, srv1 and srv2 never answer",
      PJ_FALSE, { MODE_SILENT, MODE_SILENT, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_TRUE } },
    { "UDP, all answer 503",
      PJ_FALSE, { MODE_503, MODE_503, MODE_503 }, 503,
      { PJ_TRUE, PJ_TRUE, PJ_TRUE } },
    { "UDP, caller holds the transaction (no failover)",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_TRUE },
    { "UDP, disabled: srv1 is not remembered",
      PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_TRUE, PJ_TRUE },
    { "UDP, srv1 answers 100 only (no failover)",
      PJ_FALSE, { MODE_TRYING, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_TRUE },
#if PJ_HAS_TCP
    { "TCP, srv1 answers",
      PJ_TRUE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE } },
    { "TCP, srv1 port closed",
      PJ_TRUE, { MODE_CLOSED, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE },
    { "TCP, next request skips the closed srv1",
      PJ_TRUE, { MODE_OK, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_TRUE },
    { "TCP, disabled: closed srv1 is not remembered",
      PJ_TRUE, { MODE_CLOSED, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_TRUE, PJ_TRUE },
    { "TCP, srv1 never answers",
      PJ_TRUE, { MODE_SILENT, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE } },
    { "TCP, srv1 answers 503",
      PJ_TRUE, { MODE_503, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE } },
    { "TCP, the retry keeps the public address in Via",
      PJ_TRUE, { MODE_503, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_TRUE },
    { "TCP, connection to srv1: no failover, srv1 is remembered",
      PJ_TRUE, { MODE_SILENT, MODE_OK, MODE_OK }, 408,
      { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_TRUE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_TRUE },
    { "TCP, srv1 closed, srv2 and srv3 never answer",
      PJ_TRUE, { MODE_CLOSED, MODE_SILENT, MODE_SILENT }, 408,
      { PJ_FALSE, PJ_TRUE, PJ_TRUE } },
    { "TCP, srv1 closed, srv2 answers 503",
      PJ_TRUE, { MODE_CLOSED, MODE_503, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_TRUE } },
#endif
};

/* An address listed twice, e.g: two SRV targets on the same host, is sent
 * the request once.
 */
static int dup_address_test(void)
{
    static const struct test_case tc[] =
    {
        { "UDP, srv1 listed twice: tried once",
          PJ_FALSE, { MODE_503, MODE_OK, MODE_OK }, 200,
          { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
          PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, DUP_DOMAIN },
#if PJ_HAS_TCP
        /* The transport moves on from the closed srv2 by itself */
        { "TCP, srv1 listed again after closed srv2",
          PJ_TRUE, { MODE_SILENT, MODE_CLOSED, MODE_OK }, 503,
          { PJ_TRUE, PJ_FALSE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
          PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, DUP_DOMAIN },
#endif
    };
    unsigned i;
    int rc = 0;

    for (i = 0; i < PJ_ARRAY_SIZE(tc) && rc == 0; ++i) {
        rc = run_case(&tc[i]);
        if (rc == 0 && g.srv[0].tsx_cnt != 1) {
            PJ_LOG(1,(THIS_FILE, "    error: srv1 got %d transactions",
                      g.srv[0].tsx_cnt));
            rc = -3070;
        }
    }
    return rc;
}

/* A server marked as failed while the request is being sent is tried last,
 * even when it comes before the ones tried.
 */
static int late_mark_test(void)
{
    static const struct test_case tc[] =
    {
        { "UDP, srv2 marked after sending: tried last",
          PJ_FALSE, { MODE_SILENT, MODE_OK, MODE_SILENT }, 200,
          { PJ_TRUE, PJ_TRUE, PJ_TRUE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
          PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, NULL, PJ_TRUE },
#if PJ_HAS_TCP
        /* The transport moves on from the closed srv2 by itself */
        { "TCP, srv2 marked after sending and closed",
          PJ_TRUE, { MODE_SILENT, MODE_CLOSED, MODE_SILENT }, 503,
          { PJ_TRUE, PJ_FALSE, PJ_TRUE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
          PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, NULL, PJ_TRUE },
#endif
    };
    unsigned i;
    int rc = 0;

    for (i = 0; i < PJ_ARRAY_SIZE(tc) && rc == 0; ++i)
        rc = run_case(&tc[i]);
    return rc;
}

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
            if (pjsip_endpt_is_server_failed(endpt, &addr) !=
                (i == 0 && j == 0))
            {
                PJ_LOG(1,(THIS_FILE, "    error: srv%d is %sremembered",
                          j + 1, (i == 0 && j == 0) ? "not " : ""));
                rc = -3071;
            }
        }
    }
    return rc;
}

/* The original request is kept until the callback, then released */
static int release_test(void)
{
    static const struct test_case tc =
    { "UDP, srv1 answers 503: the request is released",
      PJ_FALSE, { MODE_503, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_FALSE, PJ_FALSE,
      PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, NULL, PJ_FALSE,
      PJ_FALSE, PJ_TRUE };

    return run_case(&tc);
}

/* A request sent before the failed servers are cleared, e.g: on the previous
 * network, is not sent to another server, and its failure is not remembered.
 */
static int late_clear_test(void)
{
    struct test_case tc;
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    unsigned i;
    int rc;

    pj_bzero(&tc, sizeof(tc));
    tc.title = "UDP, sent before the failed servers are cleared";
    tc.mode[0] = MODE_SILENT;
    tc.status = 408;
    tc.reached[0] = PJ_TRUE;
    tc.srv1_first = PJ_TRUE;
    tc.late_clear = PJ_TRUE;
    rc = run_case(&tc);
    if (rc)
        return rc;

    /* Cleared during the retry: the retry says nothing about srv2 and the
     * request does not go on to srv3.
     */
    pj_bzero(&tc, sizeof(tc));
    tc.title = "UDP, cleared during the retry";
    tc.mode[0] = tc.mode[1] = MODE_SILENT;
    tc.status = 408;
    tc.reached[0] = tc.reached[1] = PJ_TRUE;
    tc.clear_on_retry = PJ_TRUE;
    rc = run_case(&tc);
    if (rc)
        return rc;
    for (i = 0; i < SRV_CNT; ++i) {
        pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[i].udp_port);
        if (pjsip_endpt_is_server_failed(endpt, &addr)) {
            PJ_LOG(1,(THIS_FILE, "    error: srv%u is remembered", i + 1));
            return -3049;
        }
    }
    return 0;
}

/* A 503 is remembered for its Retry-After, up to failed_server_timeout */
static int retry_after_limit_test(void)
{
    static const struct test_case tc =
    { "UDP, Retry-After longer than failed_server_timeout",
      PJ_FALSE, { MODE_503_RETRY, MODE_OK, MODE_OK }, 200,
      { PJ_TRUE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE };
    unsigned saved = pjsip_cfg()->endpt.failed_server_timeout;
    unsigned waited;
    int rc;

    pjsip_cfg()->endpt.failed_server_timeout = 3;
    rc = run_case(&tc);
    for (waited = 0; rc == 0 && waited < 6000; waited += 100) {
        flush_events(100);
        if (check_srv1_order(PJ_FALSE, PJ_FALSE) == 0)
            break;
    }
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
      PJ_TRUE, { MODE_HANGING, MODE_OK, MODE_OK }, 200,
      { PJ_FALSE, PJ_TRUE, PJ_FALSE }, PJ_FALSE, PJ_TRUE };

    return run_case(&tc);
}
#endif

#if PJ_HAS_TCP
/* A request too large for UDP is sent with TCP to the same addresses (RFC
 * 3261 section 18.1.1). A server that doesn't answer over TCP is remembered
 * for UDP too, and is not tried again over UDP.
 */
static int large_request_test(void)
{
    struct test_case tc;
    unsigned i;
    int rc;

    for (i = 0; i < SRV_CNT; ++i) {
        if (g.srv[i].tcp_port != g.srv[i].udp_port) {
            PJ_LOG(3,(THIS_FILE, "  large request cases skipped: the ports "
                                 "of srv%u differ", i + 1));
            return 0;
        }
    }

    pj_bzero(&tc, sizeof(tc));
    tc.title = "UDP, large request, srv1 never answers";
    tc.mode[0] = MODE_SILENT;
    tc.status = 200;
    tc.reached[0] = PJ_TRUE;
    tc.reached[1] = PJ_TRUE;
    tc.srv1_last = PJ_TRUE;
    tc.large = PJ_TRUE;
    rc = run_case(&tc);
    if (rc)
        return rc;

    pj_bzero(&tc, sizeof(tc));
    tc.title = "UDP, large request, nobody answers";
    tc.mode[0] = tc.mode[1] = tc.mode[2] = MODE_SILENT;
    tc.status = 408;
    tc.reached[0] = tc.reached[1] = tc.reached[2] = PJ_TRUE;
    tc.check_release = PJ_TRUE;
    tc.large = PJ_TRUE;
    return run_case(&tc);
}
#endif

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT
/* A server that accepts the connection but never completes the TLS
 * handshake: the request waits in the transport, which is not connected.
 * Closing the connection of srv1 once the request has moved on must not
 * disturb the others.
 */
static int tls_test(void)
{
    struct test_case tc;
    pj_str_t lo = pj_str("127.0.0.1");
    pj_sockaddr addr;
    unsigned i;
    int rc;

    if (!g.tls)
        return 0;

    pj_bzero(&tc, sizeof(tc));
    tc.title = "TLS, no handshake completes, srv1 closes after";
    tc.mode[0] = tc.mode[1] = tc.mode[2] = MODE_SILENT;
    tc.status = 408;
    /* The request stays in the transport of srv1 until the connection is
     * closed, so it can only be released after that.
     */
    tc.check_release = PJ_TRUE;
    tc.tls = PJ_TRUE;
    tc.drop_srv1 = PJ_TRUE;
    rc = run_case(&tc);
    if (rc)
        return rc;

    for (i = 0; i < SRV_CNT; ++i) {
        if (g.srv[i].accepts != 1) {
            PJ_LOG(1,(THIS_FILE, "    error: srv%u got %d connections",
                      i + 1, g.srv[i].accepts));
            return -3130;
        }
        pj_sockaddr_init(pj_AF_INET(), &addr, &lo, g.srv[i].tcp_port);
        if (!pjsip_endpt_is_server_failed(endpt, &addr)) {
            PJ_LOG(1,(THIS_FILE, "    error: srv%u is not remembered",
                      i + 1));
            return -3131;
        }
    }
    return 0;
}
#endif

#if PJ_HAS_TCP
static struct
{
    volatile pj_bool_t  done;
    int                 code;
} reg_result;

static void regc_cb(struct pjsip_regc_cbparam *param)
{
    reg_result.code = param->code;
    reg_result.done = PJ_TRUE;
}

/* The registration client reports the transport of the current attempt,
 * also once the transport has moved on to another address by itself, and
 * no longer the one the application has released.
 */
static int regc_transport_case(pj_bool_t srv2_closed)
{
    pj_str_t uri = pj_str("sip:" TEST_DOMAIN ";transport=tcp");
    pj_str_t from = pj_str("<sip:test@" TEST_DOMAIN ">");
    pj_str_t contact = pj_str("<sip:test@127.0.0.1;transport=tcp>");
    pjsip_regc *regc;
    pjsip_regc_info info;
    pjsip_tx_data *tdata;
    const pjsip_transport *seen[8];
    unsigned seen_cnt = 0, waited, i, dropped_at = 0;
    unsigned first_port = 0, last_port = 0, td;
    pj_bool_t released = PJ_FALSE;
    pj_status_t status;
    int rc = 0;

    /* With srv2 closed, srv3 answers 100 only, so that the request stays
     * on its transport for a while.
     */
    for (i = 0; i < SRV_CNT; ++i) {
        status = set_mode(&g.srv[i], i == 2 ? (srv2_closed ? MODE_TRYING :
                                                             MODE_OK) :
                                     i == 1 && srv2_closed ? MODE_CLOSED :
                                     MODE_SILENT);
        if (status != PJ_SUCCESS)
            return -3120;
    }
    td = TEST_TD + (srv2_closed ? 2 * g.refusal_msec : 0);
    pjsip_tsx_set_timers(TEST_T1, TEST_T2, TEST_T4, td);
    flush_events(200);
    forget_failed_servers(0);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;

    status = pjsip_regc_create(endpt, NULL, &regc_cb, &regc);
    if (status != PJ_SUCCESS)
        return -3121;
    status = pjsip_regc_init(regc, &uri, &from, &from, 1, &contact, 300);
    if (status == PJ_SUCCESS)
        status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status == PJ_SUCCESS) {
        /* To check that the request is released at the end */
        pjsip_tx_data_add_ref(tdata);
        pj_bzero(&reg_result, sizeof(reg_result));
        status = pjsip_regc_send(regc, tdata);
    }
    if (status != PJ_SUCCESS) {
        app_perror("    error: registering", status);
        pjsip_regc_destroy(regc);
        return -3122;
    }

    /* Note each change of the reported transport. Once the second attempt
     * is reported, srv1 closes its connection, which must not change the
     * report, then the transport of the second attempt is released. With
     * srv2 closed, the second attempt moves on to srv3 by itself.
     */
    /* Two timeouts, the second one after the refusal, and the answer */
    for (waited = 0; !reg_result.done && waited < 2 * td + 3000;
         waited += 50)
    {
        pjsip_regc_get_info(regc, &info);
        if (info.transport && !reg_result.done) {
            /* The server it is on, as pjsua looks at the transport too */
            last_port = pj_sockaddr_get_port(&info.transport->key.rem_addr);
            if (!first_port)
                first_port = last_port;
        }
        flush_events(50);
        pjsip_regc_get_info(regc, &info);
        if (seen_cnt == 0 || info.transport != seen[seen_cnt - 1]) {
            if (seen_cnt < PJ_ARRAY_SIZE(seen))
                seen[seen_cnt++] = info.transport;
            if (seen_cnt == 2 && !srv2_closed) {
                drop_connections(&g.srv[0]);
                dropped_at = waited;
            }
        }
        if (seen_cnt == 2 && !srv2_closed && !released &&
            dropped_at + 300 <= waited)
        {
            pjsip_regc_release_transport(regc);
            released = PJ_TRUE;
        }
    }
    pjsip_regc_get_info(regc, &info);

    PJ_LOG(3,(THIS_FILE, "  %-44s -> %d, %u transports reported",
              srv2_closed ? "TCP, REGISTER, srv1 never answers, srv2 closed" :
              "TCP, REGISTER, srv1 and srv2 never answer, srv1 closes",
              reg_result.done ? reg_result.code : -1, seen_cnt));
    if (!reg_result.done || reg_result.code != (srv2_closed ? 408 : 200))
        rc = -3123;
    else if (srv2_closed) {
        /* Reported on srv1 first, and on srv3 while it waits there */
        if (first_port != g.srv[0].tcp_port || last_port != g.srv[2].tcp_port)
        {
            PJ_LOG(1,(THIS_FILE, "    error: reported on port %u, then %u",
                      first_port, last_port));
            rc = -3128;
        }
    } else if (seen_cnt != 4 || !seen[0] || !seen[1] || seen[2] ||
               !seen[3] || seen[0] == seen[1] || seen[1] == seen[3])
    {
        PJ_LOG(1,(THIS_FILE, "    error: reported %p, %p, %p, %p",
                  seen_cnt > 0 ? seen[0] : NULL, seen_cnt > 1 ? seen[1] : NULL,
                  seen_cnt > 2 ? seen[2] : NULL,
                  seen_cnt > 3 ? seen[3] : NULL));
        rc = -3124;
    } else if (info.transport != seen[3]) {
        PJ_LOG(1,(THIS_FILE, "    error: the registration is on %p, not %p",
                  info.transport, seen[3]));
        rc = -3125;
    } else if (!released) {
        /* The report changed by itself when srv1 closed the connection */
        PJ_LOG(1,(THIS_FILE, "    error: the transport was not released"));
        rc = -3127;
    }

    for (waited = 0; pj_atomic_get(tdata->ref_cnt) > 1 && waited < 3000;
         waited += 50)
    {
        flush_events(50);
    }
    if (pj_atomic_get(tdata->ref_cnt) > 1) {
        PJ_LOG(1,(THIS_FILE, "    error: the request was not released"));
        rc = -3126;
    }
    pjsip_tx_data_dec_ref(tdata);
    pjsip_regc_destroy(regc);
    return rc;
}

/* The registration client reports the transport of a request sent again
 * with credentials, which is the same request, with or without failover.
 */
static int regc_auth_case(pj_bool_t failover)
{
    pj_str_t uri = pj_str("sip:" TEST_DOMAIN ";transport=tcp");
    pj_str_t from = pj_str("<sip:test@" TEST_DOMAIN ">");
    pj_str_t contact = pj_str("<sip:test@127.0.0.1;transport=tcp>");
    pjsip_cred_info cred;
    pjsip_regc *regc;
    pjsip_regc_info info;
    pjsip_tx_data *tdata;
    unsigned waited, i, port = 0, samples = 0, missing = 0;
    pj_status_t status;
    int rc = 0;

    for (i = 0; i < SRV_CNT; ++i) {
        status = set_mode(&g.srv[i], i == 0 ? MODE_401 : MODE_OK);
        if (status != PJ_SUCCESS)
            return -3150;
    }
    pjsip_tsx_set_timers(TEST_T1, TEST_T2, TEST_T4, TEST_TD);
    flush_events(200);
    forget_failed_servers(0);
    pjsip_cfg()->endpt.server_failover = failover;

    pj_bzero(&cred, sizeof(cred));
    cred.realm = pj_str(TEST_DOMAIN);
    cred.scheme = pj_str("digest");
    cred.username = pj_str("test");
    cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred.data = pj_str("secret");

    status = pjsip_regc_create(endpt, NULL, &regc_cb, &regc);
    if (status != PJ_SUCCESS)
        return -3151;
    status = pjsip_regc_init(regc, &uri, &from, &from, 1, &contact, 300);
    if (status == PJ_SUCCESS)
        status = pjsip_regc_set_credentials(regc, 1, &cred);
    if (status == PJ_SUCCESS)
        status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status == PJ_SUCCESS) {
        pj_bzero(&reg_result, sizeof(reg_result));
        status = pjsip_regc_send(regc, tdata);
    }
    if (status != PJ_SUCCESS) {
        app_perror("    error: registering", status);
        pjsip_regc_destroy(regc);
        return -3152;
    }

    /* Once the request with credentials is out, the transport must be
     * reported all the time it waits for the answer.
     */
    for (waited = 0; !reg_result.done && waited < 2 * TEST_TD + 3000;
         waited += 50)
    {
        pjsip_regc_get_info(regc, &info);
        if (g.srv[0].hits >= 2 && !reg_result.done) {
            ++samples;
            if (!info.transport)
                ++missing;
            else
                port = pj_sockaddr_get_port(&info.transport->key.rem_addr);
        }
        flush_events(50);
    }

    PJ_LOG(3,(THIS_FILE, "  %-44s -> %d, reported in %u of %u samples",
              failover ? "TCP, REGISTER, srv1 asks for credentials" :
              "TCP, REGISTER, srv1 asks for credentials, no failover",
              reg_result.done ? reg_result.code : -1, samples - missing,
              samples));
    if (!reg_result.done || reg_result.code != 408 || g.srv[0].hits != 2)
        rc = -3153;
    else if (samples == 0 || missing || port != g.srv[0].tcp_port)
        rc = -3154;

    pjsip_regc_destroy(regc);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;
    return rc;
}

static int regc_transport_test(void)
{
    int rc = regc_transport_case(PJ_FALSE);

    if (rc == 0)
        rc = regc_transport_case(PJ_TRUE);
    if (rc == 0)
        rc = regc_auth_case(PJ_TRUE);
    return rc ? rc : regc_auth_case(PJ_FALSE);
}
#endif

#if PJ_HAS_TCP
/* A request sent again with its address list, from the address that
 * answered last time, e.g: after a 401: the addresses before it were not
 * refused now.
 */
static int resent_request_test(void)
{
    pj_str_t lo = pj_str("127.0.0.1");
    pj_str_t target = pj_str("sip:" TEST_DOMAIN ";transport=tcp");
    pj_str_t from = pj_str("<sip:test@" TEST_DOMAIN ">");
    pjsip_tx_data *tdata;
    pjsip_server_addresses *addr;
    pj_sockaddr srv1;
    unsigned *token, i, waited;
    pj_status_t status;

    for (i = 0; i < SRV_CNT; ++i) {
        status = set_mode(&g.srv[i], MODE_OK);
        if (status != PJ_SUCCESS)
            return -3140;
    }
    flush_events(200);
    forget_failed_servers(0);
    pjsip_cfg()->endpt.server_failover = PJ_TRUE;

    status = pjsip_endpt_create_request(endpt, &pjsip_options_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS)
        return -3141;
    addr = &tdata->dest_info.addr;
    for (i = 0; i < 2; ++i) {
        addr->entry[i].type = PJSIP_TRANSPORT_TCP;
        pj_sockaddr_init(pj_AF_INET(), &addr->entry[i].addr, &lo,
                         g.srv[i].tcp_port);
        addr->entry[i].addr_len = sizeof(pj_sockaddr_in);
        addr->entry[i].name = pj_str(g.srv[i].name);
    }
    addr->count = 2;
    tdata->dest_info.cur_addr = 1;
    pj_sockaddr_cp(&srv1, &addr->entry[0].addr);

    token = PJ_POOL_ALLOC_T(tdata->pool, unsigned);
    *token = TOKEN_MAGIC;
    pj_bzero(&result, sizeof(result));
    status = pjsip_endpt_send_request(endpt, tdata, -1, token, &send_cb);
    if (status != PJ_SUCCESS)
        return -3142;
    for (waited = 0; !result.done && waited < 2000; waited += 50)
        flush_events(50);

    PJ_LOG(3,(THIS_FILE, "  %-44s -> %d, reached %s%s-",
              "TCP, sent again from srv2", result.done ? result.status : -1,
              g.srv[0].hits ? "1" : "-", g.srv[1].hits ? "2" : "-"));
    if (!result.done || result.status != 200 || g.srv[0].hits ||
        !g.srv[1].hits)
    {
        return -3143;
    }
    if (pjsip_endpt_is_server_failed(endpt, &srv1)) {
        PJ_LOG(1,(THIS_FILE, "    error: srv1 is remembered"));
        return -3144;
    }
    return 0;
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

    PJ_TEST_SUCCESS(pjsip_endpt_set_server_failed(endpt, &addr, 60),
                    NULL, return -3080);
    PJ_TEST_TRUE(pjsip_endpt_is_server_failed(endpt, &addr), NULL,
                 return -3081);
    /* The mark is for the port too */
    pj_sockaddr_set_port(&addr, g.srv[1].udp_port);
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, &addr), NULL,
                 return -3082);
    pj_sockaddr_set_port(&addr, g.srv[0].udp_port);
    if ((rc = check_srv1_order(PJ_FALSE, PJ_TRUE)) != 0)
        return rc - 10;

    PJ_TEST_SUCCESS(pjsip_endpt_set_server_failed(endpt, &addr, 0),
                    NULL, return -3083);
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, &addr), NULL,
                 return -3084);
    if ((rc = check_srv1_order(PJ_FALSE, PJ_FALSE)) != 0)
        return rc - 20;

    pjsip_endpt_set_server_failed(endpt, &addr, 60);
    PJ_TEST_SUCCESS(pjsip_endpt_clear_failed_servers(endpt), NULL,
                    return -3085);
    PJ_TEST_TRUE(!pjsip_endpt_is_server_failed(endpt, &addr), NULL,
                 return -3086);
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
#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT
    if (g.tls) {
        /* Let the connections that never completed fail first */
        flush_events(500);
        g.tls->destroy(g.tls);
    }
#endif
    if (g.resolver) {
        pjsip_endpt_clear_failed_servers(endpt);
        pjsip_endpt_set_resolver(endpt, g.prev_resolver);
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

    g.prev_resolver = pjsip_endpt_get_resolver(endpt);
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

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT
    {
        pjsip_tls_setting tls;
        pj_str_t lo = pj_str("127.0.0.1");
        pj_sockaddr local;

        pjsip_tls_setting_default(&tls);
        tls.verify_server = PJ_FALSE;
        pj_sockaddr_init(pj_AF_INET(), &local, &lo, 0);
        status = pjsip_tls_transport_start2(endpt, &tls, &local, NULL, 1,
                                            &g.tls);
        if (status != PJ_SUCCESS) {
            app_perror("  TLS case skipped: transport not started", status);
            g.tls = NULL;
        }
    }
#endif

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

    status = dup_address_test();
    if (status && !rc)
        rc = status;

    status = late_mark_test();
    if (status && !rc)
        rc = status;

    status = late_clear_test();
    if (status && !rc)
        rc = status;

    status = release_test();
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

#if PJ_HAS_TCP
    status = large_request_test();
    if (status && !rc)
        rc = status;

    status = regc_transport_test();
    if (status && !rc)
        rc = status;

    status = resent_request_test();
    if (status && !rc)
        rc = status;
#endif

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT
    status = tls_test();
    if (status && !rc)
        rc = status;
#endif

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


#if INCLUDE_PJSUA_ACC_TEST

/*
 * PJSUA: on an IP change, the failed servers are forgotten whatever the
 * option, including the marks set by the application.
 */
#include <pjsua-lib/pjsua.h>

/* Recreate the test framework's endpoint + tsx layer after pjsua_destroy */
static void restore_endpt(void)
{
    pj_status_t status;

    status = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (status == PJ_SUCCESS)
        status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS)
        app_perror("    error: restoring endpoint", status);
}

static int pjsua_ip_change_case(pj_bool_t failover)
{
    pjsua_config ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_ip_change_param param;
    pj_str_t ip = pj_str("192.0.2.1");
    pj_sockaddr addr;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  IP change, option %s", failover ? "on" : "off"));

    if (pjsua_create() != PJ_SUCCESS)
        return -3101;

    pjsua_config_default(&ua_cfg);
    ua_cfg.thread_cnt = 0;
    ua_cfg.server_failover = failover;
    pjsua_logging_config_default(&log_cfg);
    log_cfg.level = 3;
    log_cfg.console_level = 3;
    if (pjsua_init(&ua_cfg, &log_cfg, NULL) != PJ_SUCCESS) {
        pjsua_destroy();
        return -3102;
    }
    if (pjsua_start() != PJ_SUCCESS) {
        rc = -3103;
        goto on_return;
    }
    if (pjsip_cfg()->endpt.server_failover != failover) {
        rc = -3104;
        goto on_return;
    }

    pj_sockaddr_init(pj_AF_INET(), &addr, &ip, 5060);
    pjsip_endpt_set_server_failed(pjsua_get_pjsip_endpt(), &addr, 60);

    pjsua_ip_change_param_default(&param);
    param.restart_listener = PJ_FALSE;
    param.shutdown_transport = PJ_FALSE;
    if (pjsua_handle_ip_change(&param) != PJ_SUCCESS) {
        rc = -3105;
        goto on_return;
    }
    /* Cleared whatever the option: the application may have marked it */
    if (pjsip_endpt_is_server_failed(pjsua_get_pjsip_endpt(), &addr)) {
        rc = -3106;
        goto on_return;
    }
    if (pjsip_cfg()->endpt.server_failover != failover)
        rc = -3107;

on_return:
    pjsua_destroy();
    if (rc == 0 &&
        pjsip_cfg()->endpt.server_failover != PJSIP_SERVER_FAILOVER)
    {
        rc = -3110;
    }
    if (rc)
        PJ_LOG(1,(THIS_FILE, "    error: IP change case failed [%d]", rc));
    return rc;
}

int srv_failover_pjsua_test(void)
{
    int rc;

    PJ_LOG(3,(THIS_FILE, "PJSUA server failover test"));

    /* pjsua registers the tsx layer on its own endpoint */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    rc = pjsua_ip_change_case(PJ_TRUE);
    if (rc == 0)
        rc = pjsua_ip_change_case(PJ_FALSE);

    restore_endpt();
    return rc;
}

#endif  /* INCLUDE_PJSUA_ACC_TEST */

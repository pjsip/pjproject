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
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "transport_tcp_test.c"


/*
 * TCP transport test.
 */
#if PJ_HAS_TCP

static pj_status_t multi_listener_test(pjsip_tpfactory *factory[],
                                       unsigned num_factory,
                                       pjsip_transport *tp[],
                                       unsigned *num_tp)
{
    pj_status_t status;
    unsigned i = 0;
    pj_str_t s;
    pjsip_transport *tcp;
    pjsip_tpfactory *tpfactory = NULL;
    pj_sockaddr_in rem_addr;
    pjsip_tpselector tp_sel;
    unsigned ntp = 0;

    for (;i<num_factory;++i)
    {
        /* Start TCP listener on arbitrary port. */
        status = pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory);
        if (status != PJ_SUCCESS) {
            app_perror("   Error: unable to start TCP transport", status);
            return -10;
        }

        factory[i] = tpfactory;
    }

    /* Get the last listener address */
    status = pj_sockaddr_in_init(&rem_addr, &tpfactory->addr_name.host,
                                 (pj_uint16_t)tpfactory->addr_name.port);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: possibly invalid TCP address name", status);
        return -11;
    }

    /* Acquire transport without selector. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP,
                                           &rem_addr, sizeof(rem_addr),
                                           NULL, &tcp);
    if (status != PJ_SUCCESS || tcp == NULL) {
        app_perror("   Error: unable to acquire TCP transport", status);
        return -12;
    }
    tp[ntp++] = tcp;

    /* After pjsip_endpt_acquire_transport, TCP transport must have
     * reference counter 1.
     */
    if (pj_atomic_get(tcp->ref_cnt) != 1)
        return -13;

    /* Acquire with the same remote address, should return the same tp. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP,
                                           &rem_addr, sizeof(rem_addr),
                                           NULL, &tcp);
    if (status != PJ_SUCCESS || tcp == NULL) {
        app_perror("   Error: unable to acquire TCP transport", status);
        return -14;
    }

    /* Should return existing transport. */
    if (tp[ntp-1] != tcp) {
        return -15;
    }

    /* Using the same TCP transport, it must have reference counter 2.
     */
    if (pj_atomic_get(tcp->ref_cnt) != 2)
        return -16;

    /* Decrease the reference. */
    pjsip_transport_dec_ref(tcp);

    /* Test basic transport attributes */
    status = generic_transport_test(tcp);
    if (status != PJ_SUCCESS)
        return status;

    /* Check again that reference counter is 1. */
    if (pj_atomic_get(tcp->ref_cnt) != 1)
        return -17;

    /* Acquire transport test with selector. */
    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_LISTENER;
    tp_sel.u.listener = factory[num_factory/2];
    pj_sockaddr_in_init(&rem_addr, pj_cstr(&s, "1.1.1.1"), 80);
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP,
                                           &rem_addr, sizeof(rem_addr),
                                           &tp_sel, &tcp);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to acquire TCP transport", status);
        return -18;
    }

    /* The transport should have the same factory set on the selector. */
    if (tcp->factory != factory[num_factory/2])
        return -19;

    /* The transport should be newly created. */
    for (i = 0; i < ntp; ++i) {
        if (tp[i] == tcp) {
            break;
        }
    }
    if (i != ntp)
        return -20;

    tp[ntp++] = tcp;

    for (i = 0; i<ntp; ++i) {
        if (pj_atomic_get(tp[i]->ref_cnt) != 1)
            return -21;
    }
    *num_tp = ntp;

    return PJ_SUCCESS;
}

/*
 * Test that a failed listener restart does not unregister the factory
 * from the transport manager: occupy a port with a plain listening
 * socket, restart the listener to that port (the bind fails with
 * "address in use"), then verify that the factory is still usable
 * for creating outgoing transports.
 */
static int restart_failure_test(void)
{
    pjsip_tpfactory *tpfactory = NULL;
    pjsip_transport *tcp = NULL;
    pj_sock_t blocker = PJ_INVALID_SOCKET;
    pj_sockaddr_in blocker_addr;
    pj_sockaddr restart_addr;
    pj_sockaddr_in rem_addr;
    pj_str_t localhost = pj_str("127.0.0.1");
    pjsip_tpselector tp_sel;
    int addr_len = sizeof(blocker_addr);
    pj_status_t status;
    int ret = 0;

    /* Start TCP listener on arbitrary port. */
    status = pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to start TCP transport", status);
        return -110;
    }

    /* Occupy a port with a plain listening socket. */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &blocker);
    if (status != PJ_SUCCESS) {
        ret = -111;
        goto on_return;
    }
    pj_sockaddr_in_init(&blocker_addr, &localhost, 0);
    status = pj_sock_bind(blocker, &blocker_addr, sizeof(blocker_addr));
    if (status == PJ_SUCCESS)
        status = pj_sock_getsockname(blocker, &blocker_addr, &addr_len);
    if (status == PJ_SUCCESS)
        status = pj_sock_listen(blocker, 5);
    if (status != PJ_SUCCESS) {
        ret = -112;
        goto on_return;
    }

    /* Restart the listener to the occupied port, this must fail. */
    pj_sockaddr_init(pj_AF_INET(), &restart_addr, &localhost,
                     pj_ntohs(blocker_addr.sin_port));
    status = pjsip_tcp_transport_restart(tpfactory, &restart_addr, NULL);
    if (status == PJ_SUCCESS) {
        PJ_LOG(3,(THIS_FILE, "   restart to an occupied port unexpectedly "
                             "succeeded, skipping"));
        goto on_return;
    }

    /* The factory must still be registered to the transport manager and
     * usable for outgoing transports; connect to the blocker socket.
     */
    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_LISTENER;
    tp_sel.u.listener = tpfactory;
    pj_sockaddr_in_init(&rem_addr, &localhost,
                        pj_ntohs(blocker_addr.sin_port));
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP,
                                           &rem_addr, sizeof(rem_addr),
                                           &tp_sel, &tcp);
    if (status != PJ_SUCCESS || tcp == NULL) {
        app_perror("   Error: factory unusable after failed restart", status);
        ret = -113;
        goto on_return;
    }

    pjsip_transport_dec_ref(tcp);
    pjsip_transport_destroy(tcp);

    /* The listener can be started again once the port is free. */
    pj_sock_close(blocker);
    blocker = PJ_INVALID_SOCKET;
    status = pjsip_tcp_transport_lis_start(tpfactory, &restart_addr, NULL);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to start listener after failed restart",
                   status);
        ret = -114;
        goto on_return;
    }

    /* Verify the listener really accepts connections. */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &blocker);
    if (status == PJ_SUCCESS) {
        pj_sockaddr_in_init(&rem_addr, &localhost,
                            (pj_uint16_t)tpfactory->addr_name.port);
        status = pj_sock_connect(blocker, &rem_addr, sizeof(rem_addr));
    }
    if (status != PJ_SUCCESS) {
        app_perror("   Error: restarted listener does not accept", status);
        ret = -115;
        goto on_return;
    }

on_return:
    if (blocker != PJ_INVALID_SOCKET)
        pj_sock_close(blocker);
    if (tpfactory)
        (*tpfactory->destroy)(tpfactory);
    flush_events(500);
    return ret;
}

int transport_tcp_test(void)
{
    enum { SEND_RECV_LOOP = 8 };
    enum { NUM_LISTENER = 4 };
    enum { NUM_TP = 8 };
    pjsip_tpfactory *tpfactory[NUM_LISTENER];
    pjsip_transport *tcp[NUM_TP];
    pj_sockaddr_in rem_addr;
    pj_status_t status;
    char host_port_param[PJSIP_MAX_URL_SIZE];
    char addr[PJ_INET_ADDRSTRLEN];
    int rtt[SEND_RECV_LOOP], min_rtt;
    int pkt_lost;
    unsigned i;
    unsigned num_listener = NUM_LISTENER;
    unsigned num_tp = NUM_TP;

    status = restart_failure_test();
    if (status != 0)
        return status;

    status = multi_listener_test(tpfactory, num_listener, tcp, &num_tp);
    if (status != PJ_SUCCESS)
        return status;

    /* Get the last listener address */
    status = pj_sockaddr_in_init(&rem_addr, &tpfactory[0]->addr_name.host,
                                 (pj_uint16_t)tpfactory[0]->addr_name.port);

    pj_ansi_snprintf(host_port_param, sizeof(host_port_param),
                    "%s:%d;transport=tcp",
                    pj_inet_ntop2(pj_AF_INET(), &rem_addr.sin_addr, addr,
                                  sizeof(addr)),
                    pj_ntohs(rem_addr.sin_port));

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<SEND_RECV_LOOP; ++i) {
        status = transport_send_recv_test(PJSIP_TRANSPORT_TCP, tcp[0],
                                          host_port_param, &rtt[i]);

        if (status != 0) {
            for (i = 0; i < num_tp ; ++i) {
                pjsip_transport_dec_ref(tcp[i]);
            }
            flush_events(500);
            return -72;
        }
    }

    min_rtt = 0xFFFFFFF;
    for (i=0; i<SEND_RECV_LOOP; ++i)
        if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("tcp-rtt-usec", min_rtt, "usec",
                "Best TCP transport round trip time, in microseconds "
                "(time from sending request until response is received. "
                "Tests were performed on local machine only, and after "
                "TCP socket has been established by previous test)");


    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_TCP, tcp[0], host_port_param,
                               &pkt_lost);
    if (status != 0) {
        for (i = 0; i < num_tp ; ++i) {
            pjsip_transport_dec_ref(tcp[i]);
        }
        return status;
    }

    if (pkt_lost != 0)
        PJ_LOG(3,(THIS_FILE, "   note: %d packet(s) was lost", pkt_lost));

    /* Load test */
    if ((status=transport_load_test(PJSIP_TRANSPORT_TCP,
                                    host_port_param)) != 0)
    {
        return status;
    }

    /* Check again that reference counter is still 1. */
    for (i = 0; i < num_tp; ++i) {
        if (pj_atomic_get(tcp[i]->ref_cnt) != 1)
            return -80;
    }

    for (i = 0; i < num_tp; ++i) {
        /* Destroy this transport. */
        pjsip_transport_dec_ref(tcp[i]);

        /* Force destroy this transport. */
        status = pjsip_transport_destroy(tcp[i]);
        if (status != PJ_SUCCESS)
            return -90;
    }

    for (i = 0; i < num_listener; ++i) {
        /* Unregister factory */
        status = pjsip_tpmgr_unregister_tpfactory(pjsip_endpt_get_tpmgr(endpt),
                                                  tpfactory[i]);
        if (status != PJ_SUCCESS)
            return -95;
    }

    /* Flush events. */
    PJ_LOG(3,(THIS_FILE, "   Flushing events, 1 second..."));
    flush_events(1000);

    /* Done */
    return 0;
}


#if PJSIP_TCP_KEEP_ALIVE_RESPONSE
/* Send raw bytes and let the server process them. */
static pj_status_t ka_send_raw(pj_sock_t sock, const void *data, pj_size_t len)
{
    pj_ssize_t sent = (pj_ssize_t)len;
    pj_status_t status;

    status = pj_sock_send(sock, data, &sent, 0);
    if (status == PJ_SUCCESS && sent != (pj_ssize_t)len)
        status = PJ_ETOOSMALL;

    /* Let the server read the data and produce any response. */
    flush_events(500);
    return status;
}

/* Receive up to buf_size bytes within timeout_ms. Returns the number of bytes
 * read, 0 on timeout (nothing received), or -1 on error.
 */
static pj_ssize_t ka_recv_timeout(pj_sock_t sock, void *buf,
                                  pj_size_t buf_size, unsigned timeout_ms)
{
    pj_fd_set_t rset;
    pj_time_val timeout;
    pj_ssize_t len;
    int n;
    pj_status_t status;

    PJ_FD_ZERO(&rset);
    PJ_FD_SET(sock, &rset);
    timeout.sec = timeout_ms / 1000;
    timeout.msec = timeout_ms % 1000;

    n = pj_sock_select((int)sock + 1, &rset, NULL, NULL, &timeout);
    if (n < 0)
        return -1;
    if (n == 0 || !PJ_FD_ISSET(sock, &rset))
        return 0;

    len = (pj_ssize_t)buf_size;
    status = pj_sock_recv(sock, buf, &len, 0);
    if (status != PJ_SUCCESS)
        return -1;
    return len;
}
#endif  /* PJSIP_TCP_KEEP_ALIVE_RESPONSE */


/*
 * TCP CRLF keep-alive response test (RFC 5626 Section 4.4.1).
 *
 * Open a raw TCP connection to a PJSIP TCP listener and verify that:
 *  1. A double-CRLF keep-alive "ping" ("\r\n\r\n") is answered with a
 *     single-CRLF "pong" ("\r\n").
 *  2. A double CRLF that is NOT at the start of the stream - here it trails
 *     other data, mimicking CRLFs that are a continuation of a previous
 *     packet rather than a keep-alive ping - is NOT answered.
 *  3. After the non-ping data, a genuine ping is still answered, i.e. the
 *     stream was not left in a bad state.
 *  4. A ping fragmented across TCP reads ("\r\n"+"\r\n", and "\r"+"\n\r\n")
 *     is reassembled and answered exactly once, with no pong for the
 *     incomplete leading fragment.
 */
int transport_tcp_keep_alive_test(void)
{
#if PJSIP_TCP_KEEP_ALIVE_RESPONSE
    enum { PONG_WAIT_MSEC = 2000, NO_PONG_WAIT_MSEC = 500 };
    pjsip_tpfactory *tpfactory = NULL;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_sockaddr_in rem_addr;
    const char ping[] = { '\r', '\n', '\r', '\n' };
    /* Non-ping data whose leading byte is not CRLF, followed by a double CRLF.
     * The double CRLF here is a continuation, not a ping, so it must be
     * ignored by the keep-alive responder.
     */
    const char not_ping[] = { 'X', '\r', '\n', '\r', '\n' };
    char buf[8];
    pj_ssize_t len;
    pj_status_t status;
    int ret = 0;

    PJ_LOG(3,(THIS_FILE, "  testing TCP CRLF keep-alive response"));

    /* Start TCP listener on arbitrary port. */
    status = pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to start TCP transport", status);
        return -200;
    }

    /* Create a raw client socket and connect to the listener. */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to create socket", status);
        ret = -210;
        goto on_return;
    }

    status = pj_sockaddr_in_init(&rem_addr, &tpfactory->addr_name.host,
                                 (pj_uint16_t)tpfactory->addr_name.port);
    if (status != PJ_SUCCESS) {
        ret = -211;
        goto on_return;
    }

    status = pj_sock_connect(sock, &rem_addr, sizeof(rem_addr));
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to connect to TCP listener", status);
        ret = -212;
        goto on_return;
    }

    /* Let the server accept the connection. */
    flush_events(100);

    /* Phase 1: a plain ping must be answered with exactly one CRLF pong. */
    status = ka_send_raw(sock, ping, sizeof(ping));
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send keep-alive ping", status);
        ret = -213;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), PONG_WAIT_MSEC);
    if (len != 2 || buf[0] != '\r' || buf[1] != '\n') {
        PJ_LOG(3,(THIS_FILE, "   Error: expected a CRLF pong, got %d byte(s)",
                  (int)len));
        ret = -220;
        goto on_return;
    }

    /* Phase 2: a double CRLF that is not at the front of the stream must not
     * be treated as a ping, hence no pong.
     */
    status = ka_send_raw(sock, not_ping, sizeof(not_ping));
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send non-ping data", status);
        ret = -230;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), NO_PONG_WAIT_MSEC);
    if (len != 0) {
        PJ_LOG(3,(THIS_FILE, "   Error: got %d byte(s) response to a non-ping "
                             "double CRLF (expected none)", (int)len));
        ret = -231;
        goto on_return;
    }

    /* Phase 3: a genuine ping after the non-ping data is still answered. */
    status = ka_send_raw(sock, ping, sizeof(ping));
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send keep-alive ping", status);
        ret = -240;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), PONG_WAIT_MSEC);
    if (len != 2 || buf[0] != '\r' || buf[1] != '\n') {
        PJ_LOG(3,(THIS_FILE, "   Error: expected a CRLF pong, got %d byte(s)",
                  (int)len));
        ret = -241;
        goto on_return;
    }

    /* Phase 4-5: a ping fragmented across two reads ("\r\n" then "\r\n")
     * must be reassembled and answered once. The first fragment on its own
     * must not draw a pong, and must not be dropped either.
     */
    status = ka_send_raw(sock, ping, 2);                /* "\r\n" */
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send ping fragment", status);
        ret = -250;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), NO_PONG_WAIT_MSEC);
    if (len != 0) {
        PJ_LOG(3,(THIS_FILE, "   Error: got %d byte(s) for a partial ping "
                             "fragment (expected none)", (int)len));
        ret = -251;
        goto on_return;
    }
    status = ka_send_raw(sock, ping + 2, 2);            /* completing "\r\n" */
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send ping fragment", status);
        ret = -252;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), PONG_WAIT_MSEC);
    if (len != 2 || buf[0] != '\r' || buf[1] != '\n') {
        PJ_LOG(3,(THIS_FILE, "   Error: expected a CRLF pong for a reassembled "
                             "fragmented ping, got %d byte(s)", (int)len));
        ret = -253;
        goto on_return;
    }

    /* Phase 6-7: fragmentation on an odd boundary ("\r" then "\n\r\n"). */
    status = ka_send_raw(sock, ping, 1);                /* "\r" */
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send ping fragment", status);
        ret = -260;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), NO_PONG_WAIT_MSEC);
    if (len != 0) {
        PJ_LOG(3,(THIS_FILE, "   Error: got %d byte(s) for a 1-byte ping "
                             "fragment (expected none)", (int)len));
        ret = -261;
        goto on_return;
    }
    status = ka_send_raw(sock, ping + 1, 3);            /* "\n\r\n" */
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to send ping fragment", status);
        ret = -262;
        goto on_return;
    }
    len = ka_recv_timeout(sock, buf, sizeof(buf), PONG_WAIT_MSEC);
    if (len != 2 || buf[0] != '\r' || buf[1] != '\n') {
        PJ_LOG(3,(THIS_FILE, "   Error: expected a CRLF pong for an odd-boundary "
                             "fragmented ping, got %d byte(s)", (int)len));
        ret = -263;
        goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "   TCP keep-alive response test OK"));

on_return:
    if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);
    if (tpfactory) {
        pjsip_tpmgr_unregister_tpfactory(pjsip_endpt_get_tpmgr(endpt),
                                         tpfactory);
    }
    flush_events(500);
    return ret;
#else
    PJ_LOG(3,(THIS_FILE, "  skipping TCP CRLF keep-alive response test "
                         "(PJSIP_TCP_KEEP_ALIVE_RESPONSE=0)"));
    return 0;
#endif  /* PJSIP_TCP_KEEP_ALIVE_RESPONSE */
}


/*
 * A request too large for the receive buffer must be answered with
 * 513 Message Too Large.
 *
 * A stream message is assembled in rdata->pkt_info.packet, a fixed
 * PJSIP_MAX_PKT_LEN buffer. A request bigger than that can never be completed,
 * so the transport layer discards it. Without a response the sender has no way
 * of learning why its request went unanswered; RFC 3261 section 21.5.7
 * defines 513 for exactly this case.
 *
 * The request is written straight to a socket rather than sent through PJSIP,
 * because a pjsip_tx_data buffer is itself capped at PJSIP_MAX_PKT_LEN.
 */
static int rx_overflow_test(const char *last_hdr_eol, const char *branch)
{
    enum { TIMEOUT_MSEC = 10000, CHUNK = 1024 };
    const char *EXPECTED = "SIP/2.0 513";
    pjsip_tpfactory *tpfactory = NULL;
    pj_pool_t *pool = NULL;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_sockaddr_in rem_addr;
    pj_time_val deadline, now;
    pj_size_t body_len, buf_size, req_len, sent_total, reply_len;
    char *req;
    char reply[512];
    int len;
    int rc = 0;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  oversized request gets 513 Message Too Large "
                         "(header block ends %s)",
              (*last_hdr_eol == '\r')? "CRLF CRLF" : "LF CRLF"));

    body_len = PJSIP_MAX_PKT_LEN;
    buf_size = body_len + 1024;

    pool = pjsip_endpt_create_pool(endpt, "rxovf", buf_size + 1024, 1024);
    if (pool == NULL)
        return -500;

    /* Listener on an arbitrary port, and a plain socket connected to it. */
    status = pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to start TCP listener", status);
        rc = -505;
        goto on_return;
    }

    status = pj_sockaddr_in_init(&rem_addr, &tpfactory->addr_name.host,
                                 (pj_uint16_t)tpfactory->addr_name.port);
    if (status != PJ_SUCCESS) {
        app_perror("   error: invalid TCP address name", status);
        rc = -510;
        goto on_return;
    }

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to create socket", status);
        rc = -515;
        goto on_return;
    }

    status = pj_sock_connect(sock, &rem_addr, sizeof(rem_addr));
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to connect to TCP listener", status);
        rc = -520;
        goto on_return;
    }

    /* A body of PJSIP_MAX_PKT_LEN bytes puts the request over the receive
     * buffer whatever the header block adds.
     */
    req = (char*) pj_pool_alloc(pool, buf_size);
    len = pj_ansi_snprintf(req, buf_size,
                    "NOTIFY sip:overflow@127.0.0.1 SIP/2.0\r\n"
                    "Via: SIP/2.0/TCP 127.0.0.1:5060;branch=%s;rport\r\n"
                    "Max-Forwards: 70\r\n"
                    "From: <sip:tester@127.0.0.1>;tag=rxovf\r\n"
                    "To: <sip:overflow@127.0.0.1>\r\n"
                    "Call-ID: %s\r\n"
                    "CSeq: 1 NOTIFY\r\n"
                    "Event: dialog\r\n"
                    "Subscription-State: active;expires=60\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %u%s"
                    "\r\n",
                    branch, branch, (unsigned)body_len, last_hdr_eol);
    if (len < 0 || (pj_size_t)len + body_len > buf_size) {
        rc = -525;
        goto on_return;
    }
    pj_memset(req + len, 'x', body_len);
    req_len = (pj_size_t)len + body_len;

    /* Nothing else polls the endpoint from this thread, so let it drain the
     * connection between chunks instead of filling the socket buffer.
     */
    for (sent_total = 0; sent_total < req_len; ) {
        pj_ssize_t chunk = req_len - sent_total;
        if (chunk > CHUNK)
            chunk = CHUNK;

        status = pj_sock_send(sock, req + sent_total, &chunk, 0);
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to send the oversized request", status);
            rc = -530;
            goto on_return;
        }
        sent_total += chunk;
        flush_events(1);
    }

    pj_gettimeofday(&deadline);
    deadline.msec += TIMEOUT_MSEC;
    pj_time_val_normalize(&deadline);

    reply_len = 0;
    for (;;) {
        pj_fd_set_t rset;
        pj_time_val zero = { 0, 0 };
        pj_ssize_t received;

        flush_events(10);

        PJ_FD_ZERO(&rset);
        PJ_FD_SET(sock, &rset);
        if (pj_sock_select((int)sock + 1, &rset, NULL, NULL, &zero) > 0 &&
            PJ_FD_ISSET(sock, &rset))
        {
            received = (pj_ssize_t)(sizeof(reply) - 1 - reply_len);
            status = pj_sock_recv(sock, reply + reply_len, &received, 0);
            if (status != PJ_SUCCESS || received <= 0) {
                PJ_LOG(3,(THIS_FILE, "   error: the connection was closed "
                                     "after %u bytes, without a complete "
                                     "status line", (unsigned)reply_len));
                rc = -535;
                goto on_return;
            }
            reply_len += (pj_size_t)received;
            reply[reply_len] = '\0';

            /* A segment boundary may split the status line. */
            if (reply_len >= pj_ansi_strlen(EXPECTED))
                break;
        }

        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, deadline)) {
            PJ_LOG(3,(THIS_FILE, "   error: the oversized request was dropped "
                                 "without any response to the sender"));
            rc = -540;
            goto on_return;
        }
    }

    if (pj_ansi_strncmp(reply, EXPECTED, pj_ansi_strlen(EXPECTED)) != 0) {
        PJ_LOG(3,(THIS_FILE, "   error: expected a \"%s\" status line, got "
                             "\"%.32s\"", EXPECTED, reply));
        rc = -545;
        goto on_return;
    }

on_return:
    if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);

    if (tpfactory) {
        flush_events(100);
        pjsip_tpmgr_unregister_tpfactory(pjsip_endpt_get_tpmgr(endpt),
                                         tpfactory);
    }

    flush_events(500);

    if (pool)
        pjsip_endpt_release_pool(endpt, pool);

    return rc;
}


/*
 * Both header block terminators the parser accepts: pjsip_find_msg() ends
 * the headers at "\n\r\n", so a last header line closed by a bare LF is a
 * complete header block too.
 */
int transport_rx_overflow_test(void)
{
    int rc;

    rc = rx_overflow_test("\r\n", "z9hG4bK-rxovf-crlf");
    if (rc != 0)
        return rc;

    return rx_overflow_test("\n", "z9hG4bK-rxovf-lf");
}
#else   /* PJ_HAS_TCP */
int transport_tcp_test(void)
{
    return 0;
}
int transport_tcp_keep_alive_test(void)
{
    return 0;
}
int transport_rx_overflow_test(void)
{
    return 0;
}
#endif  /* PJ_HAS_TCP */

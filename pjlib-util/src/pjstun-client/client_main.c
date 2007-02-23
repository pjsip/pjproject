/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util.h>
#include <pjlib.h>
#include "stun_session.h"

#include <conio.h>


#define THIS_FILE	"client_main.c"


static my_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(THIS_FILE, "%s: %s", title, errmsg));
}

static pj_status_t on_send_msg(pj_stun_tx_data *tdata,
			       const void *pkt,
			       pj_size_t pkt_size,
			       unsigned addr_len, 
			       const pj_sockaddr_t *dst_addr)
{
    pj_sock_t sock;
    pj_ssize_t len;
    pj_status_t status;

    sock = (pj_sock_t) pj_stun_session_get_user_data(tdata->sess);

    len = pkt_size;
    status = pj_sock_sendto(sock, pkt, &len, 0, dst_addr, addr_len);

    if (status != PJ_SUCCESS)
	my_perror("Error sending packet", status);

    return status;
}

static void on_bind_response(pj_stun_session *sess, 
			     pj_status_t status, 
			     pj_stun_tx_data *request,
			     const pj_stun_msg *response)
{
    my_perror("on_bind_response()", status);
}

int main()
{
    pj_stun_endpoint *endpt = NULL;
    pj_pool_t *pool = NULL;
    pj_caching_pool cp;
    pj_timer_heap_t *th = NULL;
    pj_stun_session *sess;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_sockaddr_in addr;
    pj_stun_session_cb stun_cb;
    pj_stun_tx_data *tdata;
    pj_str_t s;
    pj_status_t status;

    status = pj_init();
    status = pjlib_util_init();

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    
    pool = pj_pool_create(&cp.factory, NULL, 1000, 1000, NULL);

    status = pj_timer_heap_create(pool, 1000, &th);
    pj_assert(status == PJ_SUCCESS);

    status = pj_stun_endpoint_create(&cp.factory, 0, NULL, th, &endpt);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock);
    pj_assert(status == PJ_SUCCESS);

    status = pj_sockaddr_in_init(&addr, pj_cstr(&s, "127.0.0.1"), PJ_STUN_PORT);
    pj_assert(status == PJ_SUCCESS);

    pj_memset(&stun_cb, 0, sizeof(stun_cb));
    stun_cb.on_send_msg = &on_send_msg;
    stun_cb.on_bind_response = &on_bind_response;

    status = pj_stun_session_create(endpt, NULL, &stun_cb, &sess);
    pj_assert(status == PJ_SUCCESS);

    pj_stun_session_set_user_data(sess, (void*)sock);

    status = pj_stun_session_create_bind_req(sess, &tdata);
    pj_assert(status == PJ_SUCCESS);

    status = pj_stun_session_send_msg(sess, 0, sizeof(addr), &addr, tdata);
    pj_assert(status == PJ_SUCCESS);

    while (1) {
	pj_fd_set_t rset;
	int n;
	pj_time_val timeout;

	if (kbhit()) {
	    if (_getch()==27)
		break;
	}

	PJ_FD_ZERO(&rset);
	PJ_FD_SET(sock, &rset);

	timeout.sec = 0; timeout.msec = 100;

	n = pj_sock_select(FD_SETSIZE, &rset, NULL, NULL, &timeout);

	if (PJ_FD_ISSET(sock, &rset)) {
	    char pkt[512];
	    pj_ssize_t len;

	    len = sizeof(pkt);
	    status = pj_sock_recv(sock, pkt, &len, 0);
	    if (status == PJ_SUCCESS) {
		pj_stun_session_on_rx_pkt(sess, pkt, len, NULL);
	    }
	}

	pj_timer_heap_poll(th, NULL);
    }

on_return:
    if (sock != PJ_INVALID_SOCKET)
	pj_sock_close(sock);
    if (endpt)
	pj_stun_endpoint_destroy(endpt);
    if (th)
	pj_timer_heap_destroy(th);
    if (pool)
	pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);

    return 0;
}



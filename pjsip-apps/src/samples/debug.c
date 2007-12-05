/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
 * PURPOSE:
 *   The purpose of this file is to allow debugging of a sample application
 *   using MSVC IDE.
 */

/* To debug a sample application, include the source file here.
 * E.g.:
 *  #include "playfile.c"
 */
//#include "aectest.c"
//#include "strerror.c"

#include <pjlib.h>
#include <pjlib-util.h>

#define THIS_FILE   "test.c"

void check_error(const char *func, pj_status_t status)
{
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "%s error: %s", func, errmsg));
	exit(1);
    }
}

#define DO(func)  status = func; check_error(#func, status);

int main()
{
    pj_sock_t sock;
    pj_sockaddr_in addr;
    pj_str_t stun_srv = pj_str("stun.fwdnet.net");
    pj_caching_pool cp;
    pj_status_t status;

    DO( pj_init() );

    pj_caching_pool_init(&cp, NULL, 0);

    DO( pjlib_util_init() );
    DO( pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock) );
    DO( pj_sock_bind_in(sock, 0, 0) );

    DO( pjstun_get_mapped_addr(&cp.factory, 1, &sock,
			       &stun_srv, 3478,
			       &stun_srv, 3478,
			       &addr) );

    PJ_LOG(3,(THIS_FILE, "Mapped address is %s:%d",
	      pj_inet_ntoa(addr.sin_addr),
	      (int)pj_ntohs(addr.sin_port)));

    DO( pj_sock_close(sock) );
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    return 0;
}


#if 0
#include <pjlib.h>

static void on_accept_complete(pj_ioqueue_key_t *key, 
                           pj_ioqueue_op_key_t *op_key, 
                           pj_sock_t sock, 
                           pj_status_t status)
{
}

static void on_read_complete(pj_ioqueue_key_t *key, 
                         pj_ioqueue_op_key_t *op_key, 
                         pj_ssize_t bytes_read)
{
}


int main()
{
    pj_status_t status;
    pj_caching_pool cp;
    pj_pool_t *pool;
    pj_sock_t sock, new_sock;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_op_key_t op_key;
    pj_ioqueue_callback cb;
    pj_ioqueue_key_t *key;

    status = pj_init();
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);

    pj_caching_pool_init(&cp, NULL, 0);
    pool = pj_pool_create(&cp.factory, "app", 1000, 1000, NULL);

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);
    
    status = pj_sock_bind_in(sock, 0, 80);
    if (status != PJ_SUCCESS)
	return 1;

    status = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioqueue);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);

    status = pj_sock_listen(sock, 5);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);

    pj_bzero(&cb, sizeof(cb));
    cb.on_accept_complete = &on_accept_complete;
    cb.on_read_complete = &on_read_complete;

    status = pj_ioqueue_register_sock(pool, ioqueue, sock, NULL, &cb, &key);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);

    pj_ioqueue_op_key_init(&op_key, sizeof(op_key));
    status = pj_ioqueue_accept(key, &op_key, &new_sock, NULL, NULL, NULL);
    PJ_ASSERT_RETURN(status==PJ_EPENDING, 1);
}

#endif

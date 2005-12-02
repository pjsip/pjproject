/* $Header: /pjproject/pjlib/src/test/ioqueue_test_tcp.cpp 6     5/24/05 12:17a Bennylp $
 */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/ioqueue.h>
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/log.h>
#include "libpj_test.h"
#include <stdio.h>

#if PJ_HAS_TCP

#define PORT		    50000
#define NON_EXISTANT_PORT   50000
#define LOOP		    100
#define BUF_MIN_SIZE	    32
//#define BUF_MAX_SIZE	    2048
#define BUF_MAX_SIZE	    BUF_MIN_SIZE
#define SOCK_INACTIVE_MIN   (4-2)
//#define SOCK_INACTIVE_MAX   (PJ_IOQUEUE_MAX_HANDLES - 2)
#define SOCK_INACTIVE_MAX   SOCK_INACTIVE_MIN
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)

static pj_ioqueue_key_t *polled_key;
static pj_ssize_t	 polled_size_status;
static pj_ioqueue_operation_e polled_op;

static void on_ioqueue_read(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
{
    polled_key = key;
    polled_size_status = bytes_read;
    polled_op = PJ_IOQUEUE_OP_READ;
}

static void on_ioqueue_write(pj_ioqueue_key_t *key, pj_ssize_t bytes_written)
{
    polled_key = key;
    polled_size_status = bytes_written;
    polled_op = PJ_IOQUEUE_OP_WRITE;
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, int status)
{
    polled_key = key;
    polled_size_status = status;
    polled_op = PJ_IOQUEUE_OP_ACCEPT;
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
    polled_key = key;
    polled_size_status = status;
    polled_op = PJ_IOQUEUE_OP_CONNECT;
}

static pj_ioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

static int send_recv_test(pj_ioqueue_t *ioque,
			  pj_ioqueue_key_t *skey,
			  pj_ioqueue_key_t *ckey,
			  void *send_buf,
			  void *recv_buf,
			  pj_ssize_t bufsize,
			  pj_hr_timestamp *t_elapsed)
{
    int rc;
    pj_ssize_t bytes;
    pj_str_t temp;
    pj_hr_timestamp t1, t2;
    int pending_op = 0;

    // Start reading on the server side.
    rc = pj_ioqueue_read(ioque, skey, recv_buf, bufsize);
    if (rc != 0 && rc != PJ_IOQUEUE_PENDING) {
	return -100;
    }
    
    ++pending_op;

    // Randomize send buffer.
    temp.ptr = (char*)send_buf;
    pj_create_random_string(&temp, bufsize);

    // Starts send on the client side.
    bytes = pj_ioqueue_write(ioque, ckey, send_buf, bufsize);
    if (bytes != bufsize && bytes != PJ_IOQUEUE_PENDING) {
	return -120;
    }
    if (bytes == PJ_IOQUEUE_PENDING) {
	++pending_op;
    }

    // Begin time.
    pj_hr_gettimestamp(&t1);

    // Poll the queue until we've got completion event in the server side.
    rc = 0;
    while (pending_op > 0) {
	rc = pj_ioqueue_poll(ioque, NULL);
	if (rc == 1) {
	    if (polled_size_status != bufsize) {
		return -160;
	    }
	    --pending_op;
	}
	if (rc < 0) {
	    return -170;
	}
    }

    // End time.
    pj_hr_gettimestamp(&t2);
    t_elapsed->u32.lo += (t2.u32.lo - t1.u32.lo);

    if (rc < 0) {
	return -150;
    }

    // Compare recv buffer with send buffer.
    if (pj_memcmp(send_buf, recv_buf, bufsize) != 0) {
	return -180;
    }

    // Success
    return 0;
}


/*
 * Compliance test for success scenario.
 */
static int compliance_test_0()
{
    pj_sock_t ssock=-1, csock0=-1, csock1=-1;
    pj_sockaddr_in addr, client_addr, rmt_addr;
    int client_addr_len;
    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey0, *ckey1;
    int bufsize = BUF_MIN_SIZE;
    pj_ssize_t status = -1;
    int pending_op = 0;
    pj_hr_timestamp t_elapsed;

    // Create pool.
    pool = (*mem->create_pool)(mem, NULL, POOL_SIZE, 0, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Create server socket and client socket for connecting
    ssock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, PJ_SOCK_ASYNC);
    csock1 = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, PJ_SOCK_ASYNC);
    if (ssock==PJ_INVALID_SOCKET || csock1==PJ_INVALID_SOCKET) {
	status=-1; goto on_error;
    }

    // Bind server socket.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr))) {
	status=-10; goto on_error;
    }

    // Create I/O Queue.
    ioque = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES);
    if (!ioque) {
	status=-20; goto on_error;
    }

    // Register server socket and client socket.
    skey = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)ssock, NULL, &test_cb);
    ckey1 = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)csock1, NULL, &test_cb);
    if (skey==NULL || ckey1==NULL) {
	status=-23; goto on_error;
    }

    // Server socket listen().
    if (pj_sock_listen(ssock, 5)) {
	status=-25; goto on_error;
    }

    // Server socket accept()
    client_addr_len = sizeof(pj_sockaddr_in);
    status = pj_ioqueue_accept(ioque, skey, &csock0, &client_addr, &rmt_addr, &client_addr_len);
    if (status != PJ_IOQUEUE_PENDING) {
	status=-30; goto on_error;
    }
    if (status==PJ_IOQUEUE_PENDING) {
	++pending_op;
    }

    // Initialize remote address.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Client socket connect()
    status = pj_ioqueue_connect(ioque, ckey1, &addr, sizeof(addr));
    if (status!=PJ_OK && status != PJ_IOQUEUE_PENDING) {
	status=-40; goto on_error;
    }
    if (status==PJ_IOQUEUE_PENDING) {
	++pending_op;
    }

    // Poll until connected
    while (pending_op) {
	pj_time_val timeout = {1, 0};

	status=pj_ioqueue_poll(ioque, &timeout);
	if (status==1) {
	    if (polled_op == PJ_IOQUEUE_OP_ACCEPT) {
		pj_assert(polled_key == skey);
	    } else if (polled_op == PJ_IOQUEUE_OP_CONNECT) {
		pj_assert(polled_key == ckey1);
		if (polled_size_status != 0) {
		    status = -50;
		    goto on_error;
		}
	    }
	    --pending_op;
	    if (pending_op == 0) {
		status = 0;
	    }
	}
    }

    // Check accepted socket.
    if (csock0 == PJ_INVALID_SOCKET) {
	status = -69;
	PJ_LOG(2,("tcptest", "accept() error (err %d)", pj_getlasterror()));
	goto on_error;
    }

    // Register newly accepted socket.
    ckey0 = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)csock0, NULL, &test_cb);
    if (ckey0 == NULL) {
	status = -70;
	goto on_error;
    }

    // Test send and receive.
    t_elapsed.u32.lo = 0;
    status = send_recv_test(ioque, ckey0, ckey1, send_buf, recv_buf, bufsize, &t_elapsed);
    if (status != 0) {
	goto on_error;
    }

    // Success
    status = 0;

on_error:
    if (ssock != PJ_INVALID_SOCKET)
	pj_sock_close(ssock);
    if (csock1 != PJ_INVALID_SOCKET)
	pj_sock_close(csock1);
    if (csock0 != PJ_INVALID_SOCKET)
	pj_sock_close(csock0);
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}

/*
 * Compliance test for failed scenario.
 * In this case, the client connects to a non-existant service.
 */
static int compliance_test_1()
{
    pj_sock_t csock1=-1;
    pj_sockaddr_in addr;
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *ckey1;
    pj_ssize_t status = -1;
    int pending_op = 0;

    // Create pool.
    pool = (*mem->create_pool)(mem, NULL, POOL_SIZE, 0, NULL);

    // Create I/O Queue.
    ioque = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES);
    if (!ioque) {
	status=-20; goto on_error;
    }

    // Create client socket
    csock1 = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, PJ_SOCK_ASYNC);
    if (csock1==PJ_INVALID_SOCKET) {
	status=-1; goto on_error;
    }

    // Register client socket.
    ckey1 = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)csock1, NULL, &test_cb);
    if (ckey1==NULL) {
	status=-23; goto on_error;
    }

    // Initialize remote address.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NON_EXISTANT_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Client socket connect()
    status = pj_ioqueue_connect(ioque, ckey1, &addr, sizeof(addr));
    if (status==PJ_OK) {
	// unexpectedly success!
	status = -30;
	goto on_error;
    }
    if (status != PJ_IOQUEUE_PENDING) {
	// success
    } else {
	++pending_op;
    }

    // Poll until we've got result
    while (pending_op) {
	pj_time_val timeout = {1, 0};

	status=pj_ioqueue_poll(ioque, &timeout);
	if (status==1) {
	    if (polled_op == PJ_IOQUEUE_OP_CONNECT) {
		pj_assert(polled_key == ckey1);
		if (polled_size_status == 0) {
		    // unexpectedly connected!
		    status = -50;
		    goto on_error;
		}
	    }
	    --pending_op;
	    if (pending_op == 0) {
		status = 0;
	    }
	}
    }

    // Success
    status = 0;

on_error:
    if (csock1 != PJ_INVALID_SOCKET)
	pj_sock_close(csock1);
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;
}

int tcp_ioqueue_test()
{
    int status;

    puts("..compliance test 0 (success scenario)");
    if ((status=compliance_test_0()) != 0) {
	printf("....FAILED (status=%d)\n", status);
	return status;
    }
    puts("..compliance test 1 (failed scenario)");
    if ((status=compliance_test_1()) != 0) {
	printf("....FAILED (status=%d)\n", status);
	return status;
    }

    return 0;
}

#endif	/* PJ_HAS_TCP */


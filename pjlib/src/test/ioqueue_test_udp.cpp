/* $Header: /pjproject/pjlib/src/test/ioqueue_test_udp.cpp 7     5/28/05 11:07a Bennylp $
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
#include "libpj_test.h"
#include <stdio.h>


#define PORT		    50000
#define LOOP		    100
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
//#define BUF_MAX_SIZE	    BUF_MIN_SIZE
#define SOCK_INACTIVE_MIN   (1)
#define SOCK_INACTIVE_MAX   (PJ_IOQUEUE_MAX_HANDLES - 2)
//#define SOCK_INACTIVE_MAX   SOCK_INACTIVE_MIN
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

static void on_ioqueue_write(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
{
    polled_key = key;
    polled_size_status = bytes_read;
    polled_op = PJ_IOQUEUE_OP_WRITE;
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    polled_key = key;
    polled_size_status = status;
    polled_op = PJ_IOQUEUE_OP_ACCEPT;
#else
    PJ_UNUSED_ARG(key)
    PJ_UNUSED_ARG(status)
#endif
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    polled_key = key;
    polled_size_status = status;
    polled_op = PJ_IOQUEUE_OP_CONNECT;
#else
    PJ_UNUSED_ARG(key)
    PJ_UNUSED_ARG(status)
#endif
}

static pj_ioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

static int compliance_test()
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr;
    int addrlen;
    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey;
    int bufsize = BUF_MIN_SIZE;
    pj_ssize_t bytes, status = -1;
    pj_str_t temp;

    // Create pool.
    pool = (*mem->create_pool)(mem, NULL, POOL_SIZE, 0, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    ssock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, PJ_SOCK_ASYNC);
    csock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, PJ_SOCK_ASYNC);
    if (ssock==PJ_INVALID_SOCKET || csock==PJ_INVALID_SOCKET) {
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

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    skey = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)ssock, NULL, &test_cb);
    ckey = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)csock, NULL, &test_cb);

    // Set destination address to send the packet.
    temp.ptr = "localhost";
    temp.slen = strlen(temp.ptr);
    pj_sockaddr_init(&addr, &temp, PORT);

    // Randomize send_buf.
    temp.ptr = send_buf;
    pj_create_random_string(&temp, bufsize);

    // Write must return the number of bytes.
    bytes = pj_ioqueue_sendto(ioque, ckey, send_buf, bufsize, &addr, sizeof(addr));
    if (bytes != bufsize && bytes != PJ_IOQUEUE_PENDING) {
	status=-30; goto on_error;
    } else if (bytes == PJ_IOQUEUE_PENDING) {
	puts(".....pj_ioqueue_sendto for UDP returned pending");
    } else {
	puts(".....pj_ioqueue_sendto for UDP returned immediate success");
    }

    // Read from ioqueue.
    addrlen = sizeof(addr);
    bytes = pj_ioqueue_recvfrom(ioque, skey, recv_buf, bufsize, &addr, &addrlen);
    if (bytes < 0 && bytes != PJ_IOQUEUE_PENDING) {
	status=-40; goto on_error;
    } else if (bytes == PJ_IOQUEUE_PENDING) {
	puts(".....pj_ioqueue_recvfrom for UDP returned pending");
    } else {
	puts(".....pj_ioqueue_recvfrom for UDP returned immediate success");
    }

    // Poll if pending.
    if (bytes == PJ_IOQUEUE_PENDING) {
	int rc;

	do {
	    rc = pj_ioqueue_poll(ioque, NULL);
	} while (rc==1 && polled_key != skey);

	if (rc != 1) {
	    status=-50; goto on_error;
	}
	if (polled_size_status != bufsize) {
	    status=-60; goto on_error;
	}
	if (polled_op != PJ_IOQUEUE_OP_READ) {
	    status=-65; goto on_error;
	}
	if (memcmp(send_buf, recv_buf, bufsize) != 0) {
	    printf("...ERROR: sendbuf='%s', recvbuf='%s'\n", send_buf, recv_buf);
	    status=-70; goto on_error;
	}
    } else if (bytes == bufsize) {
	if (memcmp(send_buf, recv_buf, bufsize) != 0) {
	    status=-80; goto on_error;
	}
    } else {
	status=-90; goto on_error;
    }

    // Success
    status = 0;

on_error:
    if (ssock)
	pj_sock_close(ssock);
    if (csock)
	pj_sock_close(csock);
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}

static int bench_test(int bufsize, int inactive_sock_count)
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr;
    pj_pool_t *pool = NULL;
    pj_sock_t *inactive_sock=NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey, *key;
    pj_hr_timestamp t1, t2, t_elapsed;
    int rc=0, i;
    pj_str_t temp;

    // Create pool.
    pool = (*mem->create_pool)(mem, NULL, POOL_SIZE, 0, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    ssock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, PJ_SOCK_ASYNC);
    csock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, PJ_SOCK_ASYNC);
    if (ssock==PJ_INVALID_SOCKET || csock==PJ_INVALID_SOCKET)
	goto on_error;

    // Bind server socket.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr)))
	goto on_error;

    // Create I/O Queue.
    ioque = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES);
    if (!ioque)
	goto on_error;

    // Allocate inactive sockets, and bind them to some arbitrary address.
    // Then register them to the I/O queue, and start a read operation.
    inactive_sock = (pj_sock_t*)pj_pool_alloc(pool, inactive_sock_count*sizeof(pj_sock_t));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    for (i=0; i<inactive_sock_count; ++i) {
	inactive_sock[i] = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, PJ_SOCK_ASYNC);
	if (inactive_sock[i] == PJ_INVALID_SOCKET)
	    goto on_error;
	if (pj_sock_bind(inactive_sock[i], &addr, sizeof(addr))) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    goto on_error;
	}
	key = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)inactive_sock[i], NULL, &test_cb);
	if (key==NULL) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    goto on_error;
	}
	rc = pj_ioqueue_read(ioque, key, recv_buf, bufsize);
	if ( rc < 0 && rc != PJ_IOQUEUE_PENDING) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    goto on_error;
	}
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    skey = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)ssock, NULL, &test_cb);
    ckey = pj_ioqueue_register(pool, ioque, (pj_oshandle_t)csock, NULL, &test_cb);

    // Set destination address to send the packet.
    temp.ptr = "localhost";
    temp.slen = strlen(temp.ptr);
    pj_sockaddr_init(&addr, &temp, PORT);

    // Test loop.
    t_elapsed.u32.lo = 0;
    for (i=0; i<LOOP; ++i) {
	pj_ssize_t bytes;

	// Start reading on the server side.
	rc = pj_ioqueue_read(ioque, skey, recv_buf, bufsize);
	if (rc < 0 && rc != PJ_IOQUEUE_PENDING)
	    break;

	// Randomize send buffer.
	temp.ptr = send_buf;
	pj_create_random_string(&temp, bufsize);

	// Starts send on the client side.
	bytes = pj_ioqueue_sendto(ioque, ckey, send_buf, bufsize, 
					&addr, sizeof(addr));
	if (bytes != bufsize && bytes != PJ_IOQUEUE_PENDING) {
	    rc = -1;
	    break;
	}

	// Begin time.
	pj_hr_gettimestamp(&t1);

	// Poll the queue until we've got completion event in the server side.
	do {
	    rc = pj_ioqueue_poll(ioque, NULL);
	} while (rc >= 0 && polled_key != skey);

	// End time.
	pj_hr_gettimestamp(&t2);
	t_elapsed.u32.lo += (t2.u32.lo - t1.u32.lo);

	if (rc < 0)
	    break;

	// Compare recv buffer with send buffer.
	if (polled_size_status != bufsize || memcmp(send_buf, recv_buf, bufsize)) {
	    rc = -1;
	    break;
	}

	// Poll until all events are exhausted, before we start the next loop.
	do {
	    pj_time_val timeout = { 0, 100 };
	    rc = pj_ioqueue_poll(ioque, &timeout);
	} while (rc==1);

	rc = 0;
    }

    // Print results
    if (rc == 0) {
	printf("...ok (buf:%5d, fds:%5d, elapsed:%7u ticks)\n", 
	       bufsize, inactive_sock_count+2,
	       t_elapsed.u32.lo / LOOP);
    } else {
	printf("...ERROR (buf:%d, fds:%d)\n", bufsize, inactive_sock_count+2);
    }

    // Cleaning up.
    for (i=0; i<inactive_sock_count; ++i)
	pj_sock_close(inactive_sock[i]);
    pj_sock_close(ssock);
    pj_sock_close(csock);

    pj_ioqueue_destroy(ioque);
    pj_pool_release( pool);
    return 0;

on_error:
    PJ_PERROR(("ioqueue_test", "error"));
    if (ssock)
	pj_sock_close(ssock);
    if (csock)
	pj_sock_close(csock);
    for (i=0; i<inactive_sock_count && inactive_sock && inactive_sock[i]!=PJ_INVALID_SOCKET; ++i) {
	pj_sock_close(inactive_sock[i]);
    }
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release( pool);
    return -1;
}

int udp_ioqueue_test()
{
    int status;
    int bufsize, sock_count;

    puts("...compliance test");
    if ((status=compliance_test()) != 0)
	return status;

    puts("...benchmarking different buffer size:");
    puts("... note: buf=bytes sent, fds=# of fds, elapsed=in timer ticks");
    for (bufsize=BUF_MIN_SIZE; bufsize <= BUF_MAX_SIZE; bufsize *= 2) {
	if (bench_test(bufsize, SOCK_INACTIVE_MIN))
	    return -1;
    }

    bufsize = 512;
    puts("...benchmarking multiple handles:");
    for (sock_count=SOCK_INACTIVE_MIN+2; sock_count<=SOCK_INACTIVE_MAX+2; sock_count *= 4) {
	if (bench_test(bufsize, sock_count-2))
	    return -1;
    }
    return 0;
}


/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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


/**
 * \page page_pjlib_ioqueue_udp_test Test: I/O Queue (UDP)
 *
 * This file provides implementation to test the
 * functionality of the I/O queue when UDP socket is used.
 *
 *
 * This file is <b>pjlib-test/ioq_udp.c</b>
 *
 * \include pjlib-test/ioq_udp.c
 */


#if INCLUDE_UDP_IOQUEUE_TEST

#include <pjlib.h>

#include <pj/compat/socket.h>

#define THIS_FILE	    "test_udp"
#define PORT		    51233
#define LOOP		    100
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
#define SOCK_INACTIVE_MIN   (1)
#define SOCK_INACTIVE_MAX   (PJ_IOQUEUE_MAX_HANDLES - 2)
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)

#undef TRACE_
#define TRACE_(msg)	    PJ_LOG(3,(THIS_FILE,"....." msg))

static pj_ssize_t            callback_read_size,
                             callback_write_size,
                             callback_accept_status,
                             callback_connect_status;
static pj_ioqueue_key_t     *callback_read_key,
                            *callback_write_key,
                            *callback_accept_key,
                            *callback_connect_key;
static pj_ioqueue_op_key_t  *callback_read_op,
                            *callback_write_op,
                            *callback_accept_op;

static void on_ioqueue_read(pj_ioqueue_key_t *key, 
                            pj_ioqueue_op_key_t *op_key,
                            pj_ssize_t bytes_read)
{
    callback_read_key = key;
    callback_read_op = op_key;
    callback_read_size = bytes_read;
}

static void on_ioqueue_write(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_written)
{
    callback_write_key = key;
    callback_write_op = op_key;
    callback_write_size = bytes_written;
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key,
                              pj_sock_t sock, int status)
{
    PJ_UNUSED_ARG(sock);
    callback_accept_key = key;
    callback_accept_op = op_key;
    callback_accept_status = status;
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
    callback_connect_key = key;
    callback_connect_status = status;
}

static pj_ioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

#ifdef PJ_WIN32
#  define S_ADDR S_un.S_addr
#else
#  define S_ADDR s_addr
#endif

/*
 * compliance_test()
 * To test that the basic IOQueue functionality works. It will just exchange
 * data between two sockets.
 */ 
static int compliance_test(void)
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr;
    int addrlen;
    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey;
    pj_ioqueue_op_key_t read_op, write_op;
    int bufsize = BUF_MIN_SIZE;
    pj_ssize_t bytes, status = -1;
    pj_str_t temp;
    pj_bool_t send_pending, recv_pending;
    pj_status_t rc;

    pj_set_os_error(PJ_SUCCESS);

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    TRACE_("creating sockets...");
    rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &ssock);
    if (rc==PJ_SUCCESS)
        rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &csock);
    else
        csock = PJ_INVALID_SOCKET;
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_sock_socket()", rc);
	status=-1; goto on_error;
    }

    // Bind server socket.
    TRACE_("bind socket...");
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_port = pj_htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr))) {
	status=-10; goto on_error;
    }

    // Create I/O Queue.
    TRACE_("create ioqueue...");
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
	status=-20; goto on_error;
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    TRACE_("registering first sockets...");
    rc = pj_ioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(10): ioqueue_register error", rc);
	status=-25; goto on_error;
    }
    TRACE_("registering second sockets...");
    rc = pj_ioqueue_register_sock( pool, ioque, csock, NULL, 
			           &test_cb, &ckey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(11): ioqueue_register error", rc);
	status=-26; goto on_error;
    }

    // Set destination address to send the packet.
    TRACE_("set destination address...");
    temp = pj_str("127.0.0.1");
    if ((rc=pj_sockaddr_in_init(&addr, &temp, PORT)) != 0) {
	app_perror("...error: unable to resolve 127.0.0.1", rc);
	status=-26; goto on_error;
    }

    // Randomize send_buf.
    pj_create_random_string(send_buf, bufsize);

    // Register reading from ioqueue.
    TRACE_("start recvfrom...");
    addrlen = sizeof(addr);
    bytes = bufsize;
    rc = pj_ioqueue_recvfrom(skey, &read_op, recv_buf, &bytes, 0,
			     &addr, &addrlen);
    if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
        app_perror("...error: pj_ioqueue_recvfrom", rc);
	status=-28; goto on_error;
    } else if (rc == PJ_EPENDING) {
	recv_pending = 1;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: recvfrom returned pending"));
    } else {
	PJ_LOG(3, (THIS_FILE, 
		   "......error: recvfrom returned immediate ok!"));
	status=-29; goto on_error;
    }

    // Write must return the number of bytes.
    TRACE_("start sendto...");
    bytes = bufsize;
    rc = pj_ioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0, &addr, 
			   sizeof(addr));
    if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
        app_perror("...error: pj_ioqueue_sendto", rc);
	status=-30; goto on_error;
    } else if (rc == PJ_EPENDING) {
	send_pending = 1;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: sendto returned pending"));
    } else {
	send_pending = 0;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: sendto returned immediate success"));
    }

    // reset callback variables.
    callback_read_size = callback_write_size = 0;
    callback_accept_status = callback_connect_status = -2;
    callback_read_key = callback_write_key = 
        callback_accept_key = callback_connect_key = NULL;
    callback_read_op = callback_write_op = NULL;

    // Poll if pending.
    while (send_pending || recv_pending) {
	int rc;
	pj_time_val timeout = { 5, 0 };

	TRACE_("poll...");
	rc = pj_ioqueue_poll(ioque, &timeout);

	if (rc == 0) {
	    PJ_LOG(1,(THIS_FILE, "...ERROR: timed out..."));
	    status=-45; goto on_error;
        } else if (rc < 0) {
            app_perror("...ERROR in ioqueue_poll()", rc);
	    status=-50; goto on_error;
	}

	if (callback_read_key != NULL) {
            if (callback_read_size != bufsize) {
                status=-61; goto on_error;
            }
            if (callback_read_key != skey) {
                status=-65; goto on_error;
            }
            if (callback_read_op != &read_op) {
                status=-66; goto on_error;
            }

	    if (memcmp(send_buf, recv_buf, bufsize) != 0) {
		status=-70; goto on_error;
	    }


	    recv_pending = 0;
	} 

        if (callback_write_key != NULL) {
            if (callback_write_size != bufsize) {
                status=-73; goto on_error;
            }
            if (callback_write_key != ckey) {
                status=-75; goto on_error;
            }
            if (callback_write_op != &write_op) {
                status=-76; goto on_error;
            }

            send_pending = 0;
	}
    } 
    
    // Success
    status = 0;

on_error:
    if (status != 0) {
	char errbuf[128];
	PJ_LOG(1, (THIS_FILE, 
		   "...compliance test error: status=%d, os_err=%d (%s)", 
		   status, pj_get_netos_error(),
	           pj_strerror(pj_get_netos_error(), errbuf, sizeof(errbuf))));
    }
    if (ssock)
	pj_sock_close(ssock);
    if (csock)
	pj_sock_close(csock);
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}

/*
 * Testing with many handles.
 * This will just test registering PJ_IOQUEUE_MAX_HANDLES count
 * of sockets to the ioqueue.
 */
static int many_handles_test(void)
{
    enum { MAX = PJ_IOQUEUE_MAX_HANDLES };
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_sock_t *sock;
    pj_ioqueue_key_t **key;
    pj_status_t rc;
    int count, i;

    PJ_LOG(3,(THIS_FILE,"...testing with so many handles"));

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
	return PJ_ENOMEM;

    key = pj_pool_alloc(pool, MAX*sizeof(pj_ioqueue_key_t*));
    sock = pj_pool_alloc(pool, MAX*sizeof(pj_sock_t));
    
    /* Create IOQueue */
    rc = pj_ioqueue_create(pool, MAX, &ioqueue);
    if (rc != PJ_SUCCESS || ioqueue == NULL) {
	app_perror("...error in pj_ioqueue_create", rc);
	return -10;
    }

    /* Register as many sockets. */
    for (count=0; count<MAX; ++count) {
	sock[count] = PJ_INVALID_SOCKET;
	rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[count]);
	if (rc != PJ_SUCCESS || sock[count] == PJ_INVALID_SOCKET) {
	    PJ_LOG(3,(THIS_FILE, "....unable to create %d-th socket, rc=%d", 
				 count, rc));
	    break;
	}
	key[count] = NULL;
	rc = pj_ioqueue_register_sock(pool, ioqueue, sock[count],
				      NULL, &test_cb, &key[count]);
	if (rc != PJ_SUCCESS || key[count] == NULL) {
	    PJ_LOG(3,(THIS_FILE, "....unable to register %d-th socket, rc=%d", 
				 count, rc));
	    return -30;
	}
    }

    /* Test complete. */

    /* Now deregister and close all handles. */ 

    for (i=0; i<count; ++i) {
	rc = pj_ioqueue_unregister(key[i]);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error in pj_ioqueue_unregister", rc);
	}
	rc = pj_sock_close(sock[i]);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error in pj_sock_close", rc);
	}
    }

    rc = pj_ioqueue_destroy(ioqueue);
    if (rc != PJ_SUCCESS) {
	app_perror("...error in pj_ioqueue_destroy", rc);
    }
    
    pj_pool_release(pool);

    PJ_LOG(3,(THIS_FILE,"....many_handles_test() ok"));

    return 0;
}

/*
 * Multi-operation test.
 */

/*
 * Benchmarking IOQueue
 */
static int bench_test(int bufsize, int inactive_sock_count)
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr;
    pj_pool_t *pool = NULL;
    pj_sock_t *inactive_sock=NULL;
    pj_ioqueue_op_key_t *inactive_read_op;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey, *key;
    pj_timestamp t1, t2, t_elapsed;
    int rc=0, i;
    pj_str_t temp;
    char errbuf[128];

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &ssock);
    if (rc == PJ_SUCCESS) {
        rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &csock);
    } else
        csock = PJ_INVALID_SOCKET;
    if (rc != PJ_SUCCESS) {
	app_perror("...error: pj_sock_socket()", rc);
	goto on_error;
    }

    // Bind server socket.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_port = pj_htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr)))
	goto on_error;

    pj_assert(inactive_sock_count+2 <= PJ_IOQUEUE_MAX_HANDLES);

    // Create I/O Queue.
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
	app_perror("...error: pj_ioqueue_create()", rc);
	goto on_error;
    }

    // Allocate inactive sockets, and bind them to some arbitrary address.
    // Then register them to the I/O queue, and start a read operation.
    inactive_sock = (pj_sock_t*)pj_pool_alloc(pool, 
				    inactive_sock_count*sizeof(pj_sock_t));
    inactive_read_op = (pj_ioqueue_op_key_t*)pj_pool_alloc(pool,
                              inactive_sock_count*sizeof(pj_ioqueue_op_key_t));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    for (i=0; i<inactive_sock_count; ++i) {
        pj_ssize_t bytes;

	rc = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &inactive_sock[i]);
	if (rc != PJ_SUCCESS || inactive_sock[i] < 0) {
	    app_perror("...error: pj_sock_socket()", rc);
	    goto on_error;
	}
	if ((rc=pj_sock_bind(inactive_sock[i], &addr, sizeof(addr))) != 0) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error: pj_sock_bind()", rc);
	    goto on_error;
	}
	rc = pj_ioqueue_register_sock(pool, ioque, inactive_sock[i], 
			              NULL, &test_cb, &key);
	if (rc != PJ_SUCCESS) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error(1): pj_ioqueue_register_sock()", rc);
	    PJ_LOG(3,(THIS_FILE, "....i=%d", i));
	    goto on_error;
	}
        bytes = bufsize;
	rc = pj_ioqueue_recv(key, &inactive_read_op[i], recv_buf, &bytes, 0);
	if ( rc < 0 && rc != PJ_EPENDING) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error: pj_ioqueue_read()", rc);
	    goto on_error;
	}
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    rc = pj_ioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(2): pj_ioqueue_register_sock()", rc);
	goto on_error;
    }

    rc = pj_ioqueue_register_sock(pool, ioque, csock, NULL, 
			          &test_cb, &ckey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(3): pj_ioqueue_register_sock()", rc);
	goto on_error;
    }

    // Set destination address to send the packet.
    pj_sockaddr_in_init(&addr, pj_cstr(&temp, "127.0.0.1"), PORT);

    // Test loop.
    t_elapsed.u64 = 0;
    for (i=0; i<LOOP; ++i) {
	pj_ssize_t bytes;
        pj_ioqueue_op_key_t read_op, write_op;

	// Randomize send buffer.
	pj_create_random_string(send_buf, bufsize);

	// Start reading on the server side.
        bytes = bufsize;
	rc = pj_ioqueue_recv(skey, &read_op, recv_buf, &bytes, 0);
	if (rc < 0 && rc != PJ_EPENDING) {
	    app_perror("...error: pj_ioqueue_read()", rc);
	    break;
	}

	// Starts send on the client side.
        bytes = bufsize;
	rc = pj_ioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0,
			       &addr, sizeof(addr));
	if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
	    app_perror("...error: pj_ioqueue_write()", bytes);
	    rc = -1;
	    break;
	}

	// Begin time.
	pj_get_timestamp(&t1);

	// Poll the queue until we've got completion event in the server side.
        callback_read_key = NULL;
        callback_read_size = 0;
	do {
	    rc = pj_ioqueue_poll(ioque, NULL);
	} while (rc >= 0 && callback_read_key != skey);

	// End time.
	pj_get_timestamp(&t2);
	t_elapsed.u64 += (t2.u64 - t1.u64);

	if (rc < 0)
	    break;

	// Compare recv buffer with send buffer.
	if (callback_read_size != bufsize || 
	    memcmp(send_buf, recv_buf, bufsize)) 
	{
	    rc = -1;
	    break;
	}

	// Poll until all events are exhausted, before we start the next loop.
	do {
	    pj_time_val timeout = { 0, 10 };
	    rc = pj_ioqueue_poll(ioque, &timeout);
	} while (rc>0);

	rc = 0;
    }

    // Print results
    if (rc == 0) {
	pj_timestamp tzero;
	pj_uint32_t usec_delay;

	tzero.u32.hi = tzero.u32.lo = 0;
	usec_delay = pj_elapsed_usec( &tzero, &t_elapsed);

	PJ_LOG(3, (THIS_FILE, "...%10d %15d  % 9d", 
	           bufsize, inactive_sock_count, usec_delay));

    } else {
	PJ_LOG(2, (THIS_FILE, "...ERROR (buf:%d, fds:%d)", 
			      bufsize, inactive_sock_count+2));
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
    PJ_LOG(1,(THIS_FILE, "...ERROR: %s", 
	      pj_strerror(pj_get_netos_error(), errbuf, sizeof(errbuf))));
    if (ssock)
	pj_sock_close(ssock);
    if (csock)
	pj_sock_close(csock);
    for (i=0; i<inactive_sock_count && inactive_sock && 
	      inactive_sock[i]!=PJ_INVALID_SOCKET; ++i) 
    {
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

    PJ_LOG(3, (THIS_FILE, "...compliance test (%s)", pj_ioqueue_name()));
    if ((status=compliance_test()) != 0) {
	return status;
    }
    PJ_LOG(3, (THIS_FILE, "....compliance test ok"));

    if ((status=many_handles_test()) != 0) {
	return status;
    }
    
    PJ_LOG(4, (THIS_FILE, "...benchmarking different buffer size:"));
    PJ_LOG(4, (THIS_FILE, "... note: buf=bytes sent, fds=# of fds, "
			  "elapsed=in timer ticks"));

    PJ_LOG(3, (THIS_FILE, "...Benchmarking poll times for %s:", pj_ioqueue_name()));
    PJ_LOG(3, (THIS_FILE, "...====================================="));
    PJ_LOG(3, (THIS_FILE, "...Buf.size   #inactive-socks  Time/poll"));
    PJ_LOG(3, (THIS_FILE, "... (bytes)                    (nanosec)"));
    PJ_LOG(3, (THIS_FILE, "...====================================="));

    for (bufsize=BUF_MIN_SIZE; bufsize <= BUF_MAX_SIZE; bufsize *= 2) {
	if (bench_test(bufsize, SOCK_INACTIVE_MIN))
	    return -1;
    }
    bufsize = 512;
    for (sock_count=SOCK_INACTIVE_MIN+2; 
	 sock_count<=SOCK_INACTIVE_MAX+2; 
	 sock_count *= 2) 
    {
	//PJ_LOG(3,(THIS_FILE, "...testing with %d fds", sock_count));
	if (bench_test(bufsize, sock_count-2))
	    return -1;
    }
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_udp;
#endif	/* INCLUDE_UDP_IOQUEUE_TEST */



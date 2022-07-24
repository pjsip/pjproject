/* $Id$ */
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
#include <pjlib.h>
#include <pj/compat/high_precision.h>

/**
 * \page page_pjlib_ioqueue_perf_test Test: I/O Queue Performance
 *
 * Test the performance of the I/O queue, using typical producer
 * consumer test. The test should examine the effect of using multiple
 * threads on the performance.
 *
 * This file is <b>pjlib-test/ioq_perf.c</b>
 *
 * \include pjlib-test/ioq_perf.c
 */

#if INCLUDE_IOQUEUE_PERF_TEST

#ifdef _MSC_VER
#   pragma warning ( disable: 4204)     // non-constant aggregate initializer
#endif

#define THIS_FILE	"ioq_perf"
//#define TRACE_(expr)	PJ_LOG(3,expr)
#define TRACE_(expr)


static pj_bool_t thread_quit_flag;
static pj_status_t last_error;
static unsigned last_error_counter;

/* Limit the number of send/receive to this value. The recommended value is
 * zero, to limit the test by some duration. IMO setting this value to non-zero
 * will cause the test to terminate prematurely:
 * a) setting it to 10000 (the old behavior) causes the test to complete in 1ms.
 * b) any thread that first reaches this value will cause other threads to
 *    quit, even if that other threads have not processed anything.
 */
//#define LIMIT_TRANSFER	10000
#define LIMIT_TRANSFER	0

/* Silenced error(s):
 * -Linux/Unix: EAGAIN (Resource temporarily unavailable)
 * -Windows:    WSAEWOULDBLOCK
 */
#define IS_ERROR_SILENCED(e)	((e)==PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))

/* Descriptor for each producer/consumer pair. */
typedef struct test_item
{
    const char		*type_name;
    pj_sock_t            server_fd, 
                         client_fd;
    pj_ioqueue_t        *ioqueue;
    pj_ioqueue_key_t    *server_key,
                        *client_key;
    pj_ioqueue_op_key_t  recv_op,
                         send_op;
    int                  has_pending_send;
    pj_size_t            buffer_size;
    char                *outgoing_buffer;
    char                *incoming_buffer;
    pj_size_t            bytes_sent, 
                         bytes_recv;
} test_item;

/* Callback when data has been read.
 * Increment item->bytes_recv and ready to read the next data.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_read)
{
    test_item *item = (test_item*)pj_ioqueue_get_user_data(key);
    pj_status_t rc;
    int data_is_available = 1;

    //TRACE_((THIS_FILE, "     read complete, bytes_read=%d", bytes_read));

    do {
        if (thread_quit_flag)
            return;

        if (bytes_read < 0) {
            char errmsg[PJ_ERR_MSG_SIZE];

	    rc = (pj_status_t)-bytes_read;
	    if (rc != last_error) {
	        //last_error = rc;

		/* Note:
		 * we can receive EAGAIN (on Linux/Unix) or EWOULDBLOCK (on Win)
		 * when we have more than one threads competing to do recv().
		 * This is a normal situation even when EPOLLEXCLUSIVE is used
		 * (e.g two packets arrive at the same time, causing both
		 * threads to wake up, but thread A greedily read the packets)
		 * therefore we silence the error here.
		 */
		if (!IS_ERROR_SILENCED(rc)) {
		    pj_strerror(rc, errmsg, sizeof(errmsg));
		    PJ_LOG(3,(THIS_FILE,"...error: read error, bytes_read=%d (%s)",
			      bytes_read, errmsg));
		    PJ_LOG(3,(THIS_FILE,
			      ".....additional info: type=%s, total read=%u, "
			      "total sent=%u",
			      item->type_name, item->bytes_recv,
			      item->bytes_sent));
		}
	    } else {
	        last_error_counter++;
	    }
            bytes_read = 0;

        } else if (bytes_read == 0) {
            PJ_LOG(3,(THIS_FILE, "...socket has closed!"));
        }

        item->bytes_recv += bytes_read;
    
        /* To assure that the test quits, even if main thread
         * doesn't have time to run.
         */
        if (LIMIT_TRANSFER && item->bytes_recv>item->buffer_size*LIMIT_TRANSFER)
	    thread_quit_flag = 1;

        bytes_read = item->buffer_size;
        rc = pj_ioqueue_recv( key, op_key,
                              item->incoming_buffer, &bytes_read, 0 );

        if (rc == PJ_SUCCESS) {
            data_is_available = 1;
        } else if (rc == PJ_EPENDING) {
            data_is_available = 0;
        } else {
            data_is_available = 0;
	    if (rc != last_error) {
	        last_error = rc;
	        app_perror("...error: read error(1)", rc);
	    } else {
	        last_error_counter++;
	    }
        }

        if (!item->has_pending_send) {
            pj_ssize_t sent = item->buffer_size;
            rc = pj_ioqueue_send(item->client_key, &item->send_op,
                                 item->outgoing_buffer, &sent, 0);
            if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
                app_perror("...error: write error", rc);
            } else if (rc == PJ_SUCCESS) {
        	item->bytes_sent += sent;
            }

            item->has_pending_send = (rc==PJ_EPENDING);
        }

    } while (data_is_available);
}

/* Callback when data has been written.
 * Increment item->bytes_sent and write the next data.
 */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key,
                              pj_ssize_t bytes_sent)
{
    test_item *item = (test_item*) pj_ioqueue_get_user_data(key);
    
    //TRACE_((THIS_FILE, "     write complete: sent = %d", bytes_sent));

    if (thread_quit_flag)
        return;

    if (bytes_sent <= 0) {
	if (!IS_ERROR_SILENCED(-bytes_sent)) {
	    PJ_PERROR(3, (THIS_FILE, (pj_status_t)-bytes_sent,
			  "...error: sending stopped. bytes_sent=%d",
			 -bytes_sent));
	}
	item->has_pending_send = 0;
    } 
    else if (!item->has_pending_send) {
        pj_status_t rc;

        item->bytes_sent += bytes_sent;
        bytes_sent = item->buffer_size;
        rc = pj_ioqueue_send( item->client_key, op_key,
                              item->outgoing_buffer, &bytes_sent, 0);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...error: write error", rc);
        } else if (rc == PJ_SUCCESS) {
            item->bytes_sent += bytes_sent;
        }

        item->has_pending_send = (rc==PJ_EPENDING);
    }
}

struct thread_arg
{
    int		  id;
    pj_ioqueue_t *ioqueue;
    unsigned	  loop_cnt,
		  err_cnt,
		  event_cnt;
};

/* The worker thread. */
static int worker_thread(void *p)
{
    struct thread_arg *arg = (struct thread_arg*) p;
    const pj_time_val timeout = {0, 100};
    int rc;

    while (!thread_quit_flag) {

	++arg->loop_cnt;
        rc = pj_ioqueue_poll(arg->ioqueue, &timeout);
	//TRACE_((THIS_FILE, "     thread: poll returned rc=%d", rc));
        if (rc < 0) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(-rc, errmsg, sizeof(errmsg));
            PJ_LOG(3, (THIS_FILE, 
		       "...error in pj_ioqueue_poll() in thread %d "
		       "after %d loop: %s [pj_status_t=%d]", 
		       arg->id, arg->loop_cnt, errmsg, -rc));
            //return -1;
            ++arg->err_cnt;
        } else if (rc > 0) {
            ++arg->event_cnt;
        }
    }
    return 0;
}

/* Calculate the bandwidth for the specific test configuration.
 * The test is simple:
 *  - create sockpair_cnt number of producer-consumer socket pair.
 *  - create thread_cnt number of worker threads.
 *  - each producer will send buffer_size bytes data as fast and
 *    as soon as it can.
 *  - each consumer will read buffer_size bytes of data as fast 
 *    as it could.
 *  - measure the total bytes received by all consumers during a
 *    period of time.
 */
static int perform_test(const pj_ioqueue_cfg *cfg,
			int sock_type, const char *type_name,
                        unsigned thread_cnt, unsigned sockpair_cnt,
                        pj_size_t buffer_size, 
			pj_bool_t display_report,
                        pj_size_t *p_bandwidth)
{
    enum { MSEC_DURATION = 5000 };
    pj_pool_t *pool;
    test_item *items;
    pj_thread_t **thread;
    struct thread_arg *args;
    pj_ioqueue_t *ioqueue;
    pj_status_t rc;
    pj_ioqueue_callback ioqueue_callback;
    pj_size_t total_elapsed_usec, total_received;
    pj_highprec_t bandwidth;
    pj_timestamp start, stop;
    unsigned i;

    TRACE_((THIS_FILE, "    starting test.."));

    ioqueue_callback.on_read_complete = &on_read_complete;
    ioqueue_callback.on_write_complete = &on_write_complete;

    thread_quit_flag = 0;

    pool = pj_pool_create(mem, NULL, 4096, 4096, NULL);
    if (!pool)
        return -10;

    items = (test_item*) pj_pool_calloc(pool, sockpair_cnt, sizeof(test_item));
    thread = (pj_thread_t**)
    	     pj_pool_alloc(pool, thread_cnt*sizeof(pj_thread_t*));

    TRACE_((THIS_FILE, "     creating ioqueue.."));
    rc = pj_ioqueue_create2(pool, sockpair_cnt*2, cfg, &ioqueue);
    if (rc != PJ_SUCCESS) {
        app_perror("...error: unable to create ioqueue", rc);
        return -15;
    }

    /* Initialize each producer-consumer pair. */
    for (i=0; i<sockpair_cnt; ++i) {
        pj_ssize_t bytes;

        items[i].type_name = type_name;
        items[i].ioqueue = ioqueue;
        items[i].buffer_size = buffer_size;
        items[i].outgoing_buffer = (char*) pj_pool_alloc(pool, buffer_size);
        items[i].incoming_buffer = (char*) pj_pool_alloc(pool, buffer_size);
        items[i].bytes_recv = items[i].bytes_sent = 0;

        /* randomize outgoing buffer. */
        pj_create_random_string(items[i].outgoing_buffer, buffer_size);

        /* Init operation keys. */
        pj_ioqueue_op_key_init(&items[i].recv_op, sizeof(items[i].recv_op));
        pj_ioqueue_op_key_init(&items[i].send_op, sizeof(items[i].send_op));

        /* Create socket pair. */
	TRACE_((THIS_FILE, "      calling socketpair.."));
        rc = app_socketpair(pj_AF_INET(), sock_type, 0, 
                            &items[i].server_fd, &items[i].client_fd);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: unable to create socket pair", rc);
            return -20;
        }

        /* Register server socket to ioqueue. */
	TRACE_((THIS_FILE, "      register(1).."));
        rc = pj_ioqueue_register_sock(pool, ioqueue, 
                                      items[i].server_fd,
                                      &items[i], &ioqueue_callback,
                                      &items[i].server_key);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: registering server socket to ioqueue", rc);
            return -60;
        }

        /* Register client socket to ioqueue. */
	TRACE_((THIS_FILE, "      register(2).."));
        rc = pj_ioqueue_register_sock(pool, ioqueue, 
                                      items[i].client_fd,
                                      &items[i],  &ioqueue_callback,
                                      &items[i].client_key);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: registering server socket to ioqueue", rc);
            return -70;
        }

        /* Start reading. */
	TRACE_((THIS_FILE, "      pj_ioqueue_recv.."));
        bytes = items[i].buffer_size;
        rc = pj_ioqueue_recv(items[i].server_key, &items[i].recv_op,
                             items[i].incoming_buffer, &bytes,
			     0);
        if (rc != PJ_EPENDING) {
            app_perror("...error: pj_ioqueue_recv", rc);
            return -73;
        }

        /* Start writing. */
	TRACE_((THIS_FILE, "      pj_ioqueue_write.."));
        bytes = items[i].buffer_size;
        rc = pj_ioqueue_send(items[i].client_key, &items[i].send_op,
                             items[i].outgoing_buffer, &bytes, 0);
        if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
            app_perror("...error: pj_ioqueue_write", rc);
            return -76;
        } else if (rc == PJ_SUCCESS) {
            items[i].bytes_sent += bytes;
        }

        items[i].has_pending_send = (rc==PJ_EPENDING);
    }

    /* Create the threads. */
    args = (struct thread_arg*) pj_pool_calloc(pool, thread_cnt,
					       sizeof(struct thread_arg));
    for (i=0; i<thread_cnt; ++i) {
	struct thread_arg *arg = &args[i];
	arg->id = i;
	arg->ioqueue = ioqueue;

        rc = pj_thread_create( pool, NULL, 
                               &worker_thread, 
                               arg, 
                               PJ_THREAD_DEFAULT_STACK_SIZE, 
                               PJ_THREAD_SUSPENDED, &thread[i] );
        if (rc != PJ_SUCCESS) {
            app_perror("...error: unable to create thread", rc);
            return -80;
        }
    }

    /* Mark start time. */
    rc = pj_get_timestamp(&start);
    if (rc != PJ_SUCCESS)
        return -90;

    /* Start the thread. */
    TRACE_((THIS_FILE, "     resuming all threads.."));
    for (i=0; i<thread_cnt; ++i) {
        rc = pj_thread_resume(thread[i]);
        if (rc != 0)
            return -100;
    }

    /* Wait for MSEC_DURATION seconds. 
     * This should be as simple as pj_thread_sleep(MSEC_DURATION) actually,
     * but unfortunately it doesn't work when system doesn't employ
     * timeslicing for threads.
     */
    TRACE_((THIS_FILE, "     wait for few seconds.."));
    do {
	pj_thread_sleep(1);

	/* Mark end time. */
	rc = pj_get_timestamp(&stop);

	if (thread_quit_flag) {
	    TRACE_((THIS_FILE, "      transfer limit reached.."));
	    break;
	}

	if (pj_elapsed_usec(&start,&stop) > MSEC_DURATION * 1000) {
	    TRACE_((THIS_FILE, "      time limit reached.."));
	    break;
	}

    } while (1);

    /* Terminate all threads. */
    TRACE_((THIS_FILE, "     terminating all threads.."));
    thread_quit_flag = 1;

    for (i=0; i<thread_cnt; ++i) {
	TRACE_((THIS_FILE, "      join thread %d..", i));
        pj_thread_join(thread[i]);
    }

    /* Calculate actual time in usec. */
    total_elapsed_usec = pj_elapsed_usec(&start, &stop);

    /* Close all sockets. */
    TRACE_((THIS_FILE, "     closing all sockets.."));
    for (i=0; i<sockpair_cnt; ++i) {
        pj_ioqueue_unregister(items[i].server_key);
        pj_ioqueue_unregister(items[i].client_key);
    }

    /* Destroy threads */
    for (i=0; i<thread_cnt; ++i) {
        pj_thread_destroy(thread[i]);
    }

    /* Destroy ioqueue. */
    TRACE_((THIS_FILE, "     destroying ioqueue.."));
    pj_ioqueue_destroy(ioqueue);

    /* Calculate total bytes received. */
    total_received = 0;
    for (i=0; i<sockpair_cnt; ++i) {
        total_received += items[i].bytes_recv;
    }

    /* bandwidth = total_received*1000/total_elapsed_usec */
    bandwidth = (pj_highprec_t)total_received;
    pj_highprec_mul(bandwidth, 1000);
    pj_highprec_div(bandwidth, total_elapsed_usec);
    
    *p_bandwidth = (pj_uint32_t)bandwidth;

    if (display_report) {
	PJ_LOG(3,(THIS_FILE, "  %s %d threads, %d pairs", type_name,
		  thread_cnt, sockpair_cnt));
	PJ_LOG(3,(THIS_FILE, "  Elapsed  : %u msec", total_elapsed_usec/1000));
	PJ_LOG(3,(THIS_FILE, "  Bandwidth: %d KB/s", *p_bandwidth));
	PJ_LOG(3,(THIS_FILE, "  Threads statistics:"));
	PJ_LOG(3,(THIS_FILE, "    ============================="));
	PJ_LOG(3,(THIS_FILE, "    Thread  Loops  Events  Errors"));
	PJ_LOG(3,(THIS_FILE, "    ============================="));
	for (i=0; i<thread_cnt; ++i) {
	    struct thread_arg *arg = &args[i];
	    PJ_LOG(3,(THIS_FILE, " %6d  %6d  %6d  %6d",
		      arg->id, arg->loop_cnt, arg->event_cnt, arg->err_cnt));
	}
	PJ_LOG(3,(THIS_FILE, "    ============================="));
	PJ_LOG(3,(THIS_FILE, "  Socket-pair statistics:"));
	PJ_LOG(3,(THIS_FILE, "    ==================================="));
	PJ_LOG(3,(THIS_FILE, "    Pair     Sent     Recv    Pct total"));
	PJ_LOG(3,(THIS_FILE, "    ==================================="));
	for (i=0; i<sockpair_cnt; ++i) {
	    test_item *item = &items[i];
	    PJ_LOG(3,(THIS_FILE, "    %4d  %5.1f MB  %5.1f MB    %5.1f%%",
		      i, item->bytes_sent/1000000.0,
		      item->bytes_recv/1000000.0,
		      item->bytes_recv*100.0/total_received));
	}
    } else {
	PJ_LOG(3,(THIS_FILE, "   %.4s    %2d        %2d       %8d KB/s",
		  type_name, thread_cnt, sockpair_cnt,
		  *p_bandwidth));
    }

    /* Done. */
    pj_pool_release(pool);

    TRACE_((THIS_FILE, "    done.."));
    return 0;
}

static int ioqueue_perf_test_imp(const pj_ioqueue_cfg *cfg)
{
    enum { BUF_SIZE = 512 };
    int i, rc;
    struct {
        int         type;
        const char *type_name;
        int         thread_cnt;
        int         sockpair_cnt;
    } test_param[] = 
    {
        { pj_SOCK_DGRAM(), "udp", 1, 1},
        { pj_SOCK_DGRAM(), "udp", 2, 2},
        { pj_SOCK_DGRAM(), "udp", 4, 4},
        { pj_SOCK_DGRAM(), "udp", 8, 8},
        { pj_SOCK_DGRAM(), "udp", 16, 16},

        { pj_SOCK_STREAM(), "tcp", 1, 1},
        { pj_SOCK_STREAM(), "tcp", 2, 2},
        { pj_SOCK_STREAM(), "tcp", 4, 4},
	{ pj_SOCK_STREAM(), "tcp", 8, 8},
	{ pj_SOCK_STREAM(), "tcp", 16, 16},
    };
    pj_size_t best_bandwidth;
    int best_index = 0;

    PJ_LOG(3,(THIS_FILE, " Benchmarking %s ioqueue:", pj_ioqueue_name()));
    PJ_LOG(3,(THIS_FILE, "   Testing with concurency=%d, epoll_flags=0x%x",
	      cfg->default_concurrency, cfg->epoll_flags));
    PJ_LOG(3,(THIS_FILE, "   ======================================="));
    PJ_LOG(3,(THIS_FILE, "   Type  Threads  Skt.Pairs      Bandwidth"));
    PJ_LOG(3,(THIS_FILE, "   ======================================="));

    best_bandwidth = 0;
    for (i=0; i<(int)(sizeof(test_param)/sizeof(test_param[0])); ++i) {
        pj_size_t bandwidth;

        rc = perform_test(cfg,
			  test_param[i].type, 
                          test_param[i].type_name,
                          test_param[i].thread_cnt, 
                          test_param[i].sockpair_cnt, 
                          BUF_SIZE, 
			  PJ_FALSE,
                          &bandwidth);
        if (rc != 0)
            return rc;

        if (bandwidth > best_bandwidth)
            best_bandwidth = bandwidth, best_index = i;

        /* Give it a rest before next test, to allow system to close the
	 * sockets properly. 
	 */
        pj_thread_sleep(500);
    }

    PJ_LOG(3,(THIS_FILE, 
              "   Best: Type=%s Threads=%d, Skt.Pairs=%d, Bandwidth=%u KB/s",
              test_param[best_index].type_name,
              test_param[best_index].thread_cnt,
              test_param[best_index].sockpair_cnt,
              best_bandwidth));
    PJ_LOG(3,(THIS_FILE, "   (Note: packet size=%d, total errors=%u)", 
			 BUF_SIZE, last_error_counter));
    return 0;
}

/*
 * main test entry.
 */
int ioqueue_perf_test(void)
{
    pj_ioqueue_epoll_flag epoll_flags[] = {
#if PJ_HAS_LINUX_EPOLL
	PJ_IOQUEUE_EPOLL_EXCLUSIVE,
	PJ_IOQUEUE_EPOLL_ONESHOT,
	0,
#else
        PJ_IOQUEUE_EPOLL_AUTO,
#endif
    };
    pj_size_t bandwidth;
    pj_ioqueue_cfg cfg;
    int i, rc;

    /* Defailed performance report (concurrency=1) */
    for (i=0; i<PJ_ARRAY_SIZE(epoll_flags); ++i) {
	pj_ioqueue_cfg_default(&cfg);
	cfg.epoll_flags = epoll_flags[i];

	PJ_LOG(3,(THIS_FILE, " Detailed perf (concurrency=%d, epoll_flags=0x%x):",
		  cfg.default_concurrency, cfg.epoll_flags));
	rc = perform_test(&cfg,
			  pj_SOCK_DGRAM(),
			  "udp",
			  8,
			  8,
			  512,
			  PJ_TRUE,
			  &bandwidth);
	if (rc != 0)
	    return rc;
    }

    /* Defailed performance report (concurrency=0) */
    pj_ioqueue_cfg_default(&cfg);
    cfg.default_concurrency = PJ_FALSE;
    PJ_LOG(3,(THIS_FILE, " Detailed perf (concurrency=%d, epoll_flags=0x%x):",
	      cfg.default_concurrency, cfg.epoll_flags));
    rc = perform_test(&cfg,
		      pj_SOCK_DGRAM(),
                      "udp",
                      8,
                      8,
                      512,
		      PJ_TRUE,
                      &bandwidth);
    if (rc != 0)
	return rc;

    /* The benchmark across configs */
    for (i=0; i<PJ_ARRAY_SIZE(epoll_flags); ++i) {
	int concur;
	for (concur=0; concur<2; ++concur) {
	    pj_ioqueue_cfg_default(&cfg);
	    cfg.epoll_flags = epoll_flags[i];
	    cfg.default_concurrency = concur;
	    rc = ioqueue_perf_test_imp(&cfg);
	    if (rc != 0)
		return rc;
	}
    }

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_perf_test;
#endif  /* INCLUDE_IOQUEUE_PERF_TEST */



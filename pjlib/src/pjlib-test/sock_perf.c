/* $Id$
 *
 */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/sock_perf.c $
 * 
 * 4     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 3     14/10/05 11:31 Bennylp
 * Fixed bug when TCP data is part_received in chunks.
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/11/05 11:18p Bennylp
 * Created.
 *
 */
#include "test.h"
#include <pjlib.h>
#include <pj/compat/high_precision.h>


/**
 * \page page_pjlib_sock_perf_test Test: Socket Performance
 *
 * Test the performance of the socket communication. This will perform
 * simple producer-consumer type of test, where we calculate how long
 * does it take to send certain number of packets from producer to
 * consumer.
 *
 * This file is <b>pjlib-test/sock_perf.c</b>
 *
 * \include pjlib-test/sock_perf.c
 */

#if INCLUDE_SOCK_PERF_TEST

/*
 * sock_producer_consumer()
 *
 * Simple producer-consumer benchmarking. Send loop number of
 * buf_size size packets as fast as possible.
 */
static int sock_producer_consumer(int sock_type,
                                  unsigned buf_size,
                                  unsigned loop, 
                                  unsigned *p_bandwidth)
{
    pj_sock_t consumer, producer;
    pj_pool_t *pool;
    char *outgoing_buffer, *incoming_buffer;
    pj_timestamp start, stop;
    unsigned i;
    pj_highprec_t elapsed, bandwidth;
    pj_size_t total_received;
    pj_status_t rc;

    /* Create pool. */
    pool = pj_pool_create(mem, NULL, 4096, 4096, NULL);
    if (!pool)
        return -10;

    /* Create producer-consumer pair. */
    rc = app_socketpair(PJ_AF_INET, sock_type, 0, &consumer, &producer);
    if (rc != PJ_SUCCESS) {
        app_perror("...error: create socket pair", rc);
        return -20;
    }

    /* Create buffers. */
    outgoing_buffer = pj_pool_alloc(pool, buf_size);
    incoming_buffer = pj_pool_alloc(pool, buf_size);

    /* Start loop. */
    pj_get_timestamp(&start);
    total_received = 0;
    for (i=0; i<loop; ++i) {
        pj_ssize_t sent, part_received, received;
	pj_time_val delay;

        sent = buf_size;
        rc = pj_sock_send(producer, outgoing_buffer, &sent, 0);
        if (rc != PJ_SUCCESS || sent != (pj_ssize_t)buf_size) {
            app_perror("...error: send()", rc);
            return -61;
        }

        /* Repeat recv() until all data is part_received.
         * This applies only for non-UDP of course, since for UDP
         * we would expect all data to be part_received in one packet.
         */
        received = 0;
        do {
            part_received = buf_size-received;
	    rc = pj_sock_recv(consumer, incoming_buffer+received, 
			      &part_received, 0);
	    if (rc != PJ_SUCCESS) {
	        app_perror("...recv error", rc);
	        return -70;
	    }
            if (part_received <= 0) {
                PJ_LOG(3,("", "...error: socket has closed (part_received=%d)!",
                          part_received));
                return -73;
            }
	    if ((pj_size_t)part_received != buf_size-received) {
                if (sock_type != PJ_SOCK_STREAM) {
	            PJ_LOG(3,("", "...error: expecting %u bytes, got %u bytes",
                              buf_size-received, part_received));
	            return -76;
                }
	    }
            received += part_received;
        } while ((pj_size_t)received < buf_size);

	total_received += received;

	/* Stop test if it's been runnign for more than 10 secs. */
	pj_get_timestamp(&stop);
	delay = pj_elapsed_time(&start, &stop);
	if (delay.sec > 10)
	    break;
    }

    /* Stop timer. */
    pj_get_timestamp(&stop);

    elapsed = pj_elapsed_usec(&start, &stop);

    /* bandwidth = total_received * 1000 / elapsed */
    bandwidth = total_received;
    pj_highprec_mul(bandwidth, 1000);
    pj_highprec_div(bandwidth, elapsed);
    
    *p_bandwidth = (pj_uint32_t)bandwidth;

    /* Close sockets. */
    pj_sock_close(consumer);
    pj_sock_close(producer);

    /* Done */
    pj_pool_release(pool);

    return 0;
}

/*
 * sock_perf_test()
 *
 * Main test entry.
 */
int sock_perf_test(void)
{
    enum { LOOP = 64 * 1024 };
    int rc;
    unsigned bandwidth;

    PJ_LOG(3,("", "...benchmarking socket "
                  "(2 sockets, packet=512, single threaded):"));
    
    /* Benchmarking UDP */
    rc = sock_producer_consumer(PJ_SOCK_DGRAM, 512, LOOP, &bandwidth);
    if (rc != 0) return rc;
    PJ_LOG(3,("", "....bandwidth UDP = %d KB/s", bandwidth));

    /* Benchmarking TCP */
    rc = sock_producer_consumer(PJ_SOCK_STREAM, 512, LOOP, &bandwidth);
    if (rc != 0) return rc;
    PJ_LOG(3,("", "....bandwidth TCP = %d KB/s", bandwidth));

    return rc;
}


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_sock_perf_test;
#endif  /* INCLUDE_SOCK_PERF_TEST */



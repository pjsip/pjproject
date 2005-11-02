/* $Id$
 */
#include "test.h"
#include <pjlib.h>

static pj_sem_t    *sem;
static pj_mutex_t  *mutex;
static pj_size_t    total_bw;

static int worker_thread(void *arg)
{
    pj_sock_t    sock = (pj_sock_t)arg;
    char         buf[1516];
    pj_size_t    received;
    pj_time_val  last_print;
    pj_status_t  last_recv_err = PJ_SUCCESS, last_write_err = PJ_SUCCESS;

    received = 0;
    pj_gettimeofday(&last_print);

    for (;;) {
        pj_ssize_t len;
        pj_uint32_t delay_msec;
        pj_time_val now;
        pj_highprec_t bw;
        pj_status_t rc;
        pj_sockaddr_in addr;
        int addrlen;

        len = sizeof(buf);
        addrlen = sizeof(addr);
        rc = pj_sock_recvfrom(sock, buf, &len, 0, &addr, &addrlen);
        if (rc != 0) {
            if (rc != last_recv_err) {
                app_perror("...recv error", rc);
                last_recv_err = rc;
            }
            continue;
        }

        received += len;

        rc = pj_sock_sendto(sock, buf, &len, 0, &addr, addrlen);
        if (rc != PJ_SUCCESS) {
            if (rc != last_write_err) {
                app_perror("...send error", rc);
                last_write_err = rc;
            }
            continue;
        }

        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, last_print);
        delay_msec = PJ_TIME_VAL_MSEC(now);

        if (delay_msec < 1000)
            continue;
 
        bw = received;
        pj_highprec_mul(bw, 1000);
        pj_highprec_div(bw, delay_msec);

        pj_mutex_lock(mutex);
        total_bw = total_bw + (pj_size_t)bw;
        pj_mutex_unlock(mutex);

        pj_gettimeofday(&last_print);
        received = 0;
        pj_sem_post(sem);
        pj_thread_sleep(0);
    }
}


int echo_srv_sync(void)
{
    pj_pool_t *pool;
    pj_sock_t sock;
    pj_thread_t *thread[ECHO_SERVER_MAX_THREADS];
    pj_status_t rc;
    pj_highprec_t abs_total;
    unsigned count;
    int i;

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -5;

    rc = pj_sem_create(pool, NULL, 0, ECHO_SERVER_MAX_THREADS, &sem);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to create semaphore", rc);
        return -6;
    }

    rc = pj_mutex_create_simple(pool, NULL, &mutex);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to create mutex", rc);
        return -7;
    }

    rc = app_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, ECHO_SERVER_START_PORT, &sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...socket error", rc);
        return -10;
    }

    for (i=0; i<ECHO_SERVER_MAX_THREADS; ++i) {
        rc = pj_thread_create(pool, NULL, &worker_thread, (void*)sock,
                              PJ_THREAD_DEFAULT_STACK_SIZE, 0,
                              &thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...unable to create thread", rc);
            return -20;
        }
    }

    PJ_LOG(3,("", "...UDP echo server running with %d threads at port %d",
                  ECHO_SERVER_MAX_THREADS, ECHO_SERVER_START_PORT));
    PJ_LOG(3,("", "...Press Ctrl-C to abort"));

    abs_total = 0;
    count = 0;

    for (;;) {
        pj_uint32_t avg32;
        pj_highprec_t avg;

        for (i=0; i<ECHO_SERVER_MAX_THREADS; ++i)
            pj_sem_wait(sem);

        /* calculate average so far:
           avg = abs_total / count;
         */
        count++;
        abs_total += total_bw;
        avg = abs_total;
        pj_highprec_div(avg, count);
        avg32 = (pj_uint32_t)avg;

        
        PJ_LOG(3,("", "Synchronous UDP (%d threads): %u KB/s  (avg=%u KB/s) %s", 
                  ECHO_SERVER_MAX_THREADS, 
                  total_bw / 1000,
                  avg32 / 1000,
                  (count==20 ? "<ses avg>" : "")));

        total_bw = 0;

        if (count==20) {
            count = 0;
            abs_total = 0;
        }

        while (pj_sem_trywait(sem) == PJ_SUCCESS)
            ;
    }
}



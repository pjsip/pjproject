/* $Id$
 *
 */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/echo_clt.c $
 * 
 * 3     10/29/05 10:25p Bennylp
 * Tested.
 * 
 * 2     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 1     10/24/05 11:28a Bennylp
 * Created.
 *
 */
#include "test.h"
#include <pjlib.h>

#if INCLUDE_ECHO_CLIENT

enum { BUF_SIZE = 512 };

struct client
{
    int sock_type;
    const char *server;
    int port;
};

static pj_sem_t *sem;
static pj_mutex_t *mutex;
static pj_size_t total_bw;
static unsigned total_poster;
static pj_time_val first_report;

#define MSEC_PRINT_DURATION 1000

static int wait_socket(pj_sock_t sock, unsigned msec_timeout)
{
    pj_fd_set_t fdset;
    pj_time_val timeout;

    timeout.sec = 0;
    timeout.msec = msec_timeout;
    pj_time_val_normalize(&timeout);

    PJ_FD_ZERO(&fdset);
    PJ_FD_SET(sock, &fdset);
    
    return pj_sock_select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
}

static int echo_client_thread(void *arg)
{
    pj_sock_t sock;
    char send_buf[BUF_SIZE];
    char recv_buf[BUF_SIZE];
    pj_sockaddr_in addr;
    pj_str_t s;
    pj_status_t rc;
    struct client *client = arg;
    pj_status_t last_recv_err = PJ_SUCCESS, last_send_err = PJ_SUCCESS;

    pj_time_val last_report, next_report;
    pj_size_t thread_total;

    rc = app_socket(PJ_AF_INET, client->sock_type, 0, -1, &sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to create socket", rc);
        return -10;
    }

    rc = pj_sockaddr_in_init( &addr, pj_cstr(&s, client->server), 
                              (pj_uint16_t)client->port);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to resolve server", rc);
        return -15;
    }

    rc = pj_sock_connect(sock, &addr, sizeof(addr));
    if (rc != PJ_SUCCESS) {
        app_perror("...connect() error", rc);
        pj_sock_close(sock);
        return -20;
    }

    PJ_LOG(3,("", "...socket connected to %s:%d", 
		  pj_inet_ntoa(addr.sin_addr),
		  pj_ntohs(addr.sin_port)));

    pj_create_random_string(send_buf, BUF_SIZE);
    thread_total = 0;

    /* Give other thread chance to initialize themselves! */
    pj_thread_sleep(500);

    pj_gettimeofday(&last_report);
    next_report = first_report;

    //PJ_LOG(3,("", "...thread %p running", pj_thread_this()));

    for (;;) {
        int rc;
        pj_ssize_t bytes;
        pj_time_val now;

        /* Send a packet. */
        bytes = BUF_SIZE;
        rc = pj_sock_send(sock, send_buf, &bytes, 0);
        if (rc != PJ_SUCCESS || bytes != BUF_SIZE) {
            if (rc != last_send_err) {
                app_perror("...send() error", rc);
                PJ_LOG(3,("", "...ignoring subsequent error.."));
                last_send_err = rc;
                pj_thread_sleep(100);
            }
            continue;
        }

        rc = wait_socket(sock, 500);
        if (rc == 0) {
            PJ_LOG(3,("", "...timeout"));
	    bytes = 0;
	} else if (rc < 0) {
	    rc = pj_get_netos_error();
	    app_perror("...select() error", rc);
	    break;
        } else {
            /* Receive back the original packet. */
            bytes = 0;
            do {
                pj_ssize_t received = BUF_SIZE - bytes;
                rc = pj_sock_recv(sock, recv_buf+bytes, &received, 0);
                if (rc != PJ_SUCCESS || received == 0) {
                    if (rc != last_recv_err) {
                        app_perror("...recv() error", rc);
                        PJ_LOG(3,("", "...ignoring subsequent error.."));
                        last_recv_err = rc;
                        pj_thread_sleep(100);
                    }
                    bytes = 0;
		    received = 0;
                    break;
                }
                bytes += received;
            } while (bytes != BUF_SIZE && bytes != 0);
        }

        /* Accumulate total received. */
        thread_total = thread_total + bytes;

        /* Report current bandwidth on due. */
        pj_gettimeofday(&now);

        if (PJ_TIME_VAL_GTE(now, next_report)) {
            pj_uint32_t bw;
            pj_bool_t signal_parent = 0;
            pj_time_val duration;
            pj_uint32_t msec;

            duration = now;
            PJ_TIME_VAL_SUB(duration, last_report);
            msec = PJ_TIME_VAL_MSEC(duration);

            bw = thread_total * 1000 / msec;

            /* Post result to parent */
            pj_mutex_lock(mutex);
            total_bw += bw;
            total_poster++;
            //PJ_LOG(3,("", "...thread %p posting result", pj_thread_this()));
            if (total_poster >= ECHO_CLIENT_MAX_THREADS)
                signal_parent = 1;
            pj_mutex_unlock(mutex);

            thread_total = 0;
            last_report = now;
            next_report.sec++;

            if (signal_parent) {
                pj_sem_post(sem);
            }

            pj_thread_sleep(0);
        }

        if (bytes == 0)
            continue;

        if (pj_memcmp(send_buf, recv_buf, BUF_SIZE) != 0) {
            //PJ_LOG(3,("", "...error: buffer has changed!"));
            break;
        }
    }

    pj_sock_close(sock);
    return 0;
}

int echo_client(int sock_type, const char *server, int port)
{
    pj_pool_t *pool;
    pj_thread_t *thread[ECHO_CLIENT_MAX_THREADS];
    pj_status_t rc;
    struct client client;
    int i;

    client.sock_type = sock_type;
    client.server = server;
    client.port = port;

    pool = pj_pool_create( mem, NULL, 4000, 4000, NULL );

    rc = pj_sem_create(pool, NULL, 0, ECHO_CLIENT_MAX_THREADS+1, &sem);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,("", "...error: unable to create semaphore", rc));
        return -10;
    }

    rc = pj_mutex_create_simple(pool, NULL, &mutex);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,("", "...error: unable to create mutex", rc));
        return -20;
    }

    /*
    rc = pj_atomic_create(pool, 0, &atom);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,("", "...error: unable to create atomic variable", rc));
        return -30;
    }
    */

    PJ_LOG(3,("", "Echo client started"));
    PJ_LOG(3,("", "  Destination: %s:%d", 
                  ECHO_SERVER_ADDRESS, ECHO_SERVER_START_PORT));
    PJ_LOG(3,("", "  Press Ctrl-C to exit"));

    pj_gettimeofday(&first_report);
    first_report.sec += 2;

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        rc = pj_thread_create( pool, NULL, &echo_client_thread, &client, 
                               PJ_THREAD_DEFAULT_STACK_SIZE, 0,
                               &thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: unable to create thread", rc);
            return -10;
        }
    }

    for (;;) {
        pj_uint32_t bw;

        pj_sem_wait(sem);

        pj_mutex_lock(mutex);
        bw = total_bw;
        total_bw = 0;
        total_poster = 0;
        pj_mutex_unlock(mutex);

        PJ_LOG(3,("", "...%d threads, total bandwidth: %d KB/s", 
                  ECHO_CLIENT_MAX_THREADS, bw/1000));
    }

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        pj_thread_join( thread[i] );
    }

    pj_pool_release(pool);
    return 0;
}


#else
int dummy_echo_client;
#endif  /* INCLUDE_ECHO_CLIENT */
